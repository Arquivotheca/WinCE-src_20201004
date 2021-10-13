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
/*++


Module Name:

    exdsptch.c

Abstract:

    This module implements the dispatching of exception and the unwinding of
    procedure call frames.

--*/

#include "kernel.h"
#include "excpt.h"
#include "pager.h"

FARPROC g_pfnKrnRtlDispExcp;
FARPROC g_pfnUsrRtlDispExcp;

#ifndef x86
FARPROC g_pfnRtlUnwindOneFrame;
#endif

ERRFALSE(offsetof (CallSnapshotEx, dwReturnAddr) == 0);
ERRFALSE(offsetof (CallSnapshot3, dwReturnAddr) == 0);

ERRFALSE(offsetof (CallSnapshot3, dwFramePtr) == offsetof (CallSnapshotEx, dwFramePtr));
ERRFALSE(offsetof (CallSnapshot3, dwActvProc) == offsetof (CallSnapshotEx, dwCurProc));



// Obtain current stack limits
//  if we're on original stack, or if there is a secure stack (pth->tlsSecure != pth->tlsNonSecure)
//
static BOOL GetStackLimitsFromTLS (LPDWORD tlsPtr, LPDWORD pLL, LPDWORD pHL)
{
    *pLL = tlsPtr[PRETLS_STACKBASE];
    *pHL = tlsPtr[PRETLS_STACKBASE] + tlsPtr[PRETLS_STACKSIZE] - 4 * REG_SIZE;

    return (IsKModeAddr ((DWORD) tlsPtr)       // on secure stack - trust it
            || (   IsValidUsrPtr ((LPVOID) (*pLL), sizeof (DWORD), TRUE)    // lower-bound is a user address
                && IsValidUsrPtr ((LPVOID) (*pHL), sizeof (DWORD), TRUE)    // upper-bound is a user address
                && (*pHL > *pLL)));                                         // upper bound > lower bound
}

static PCALLSTACK ReserveCSTK (DWORD dwCtxSP, DWORD dwMode)
{
    PTHREAD    pCurTh = pCurThread;
    PCALLSTACK pcstk = pCurTh->pcstkTop;

    DWORD dwSP = (USER_MODE != dwMode)
                ? dwCtxSP
                : (pcstk
                        ? (DWORD) pcstk
                        : ((DWORD) pCurTh->tlsSecure - SECURESTK_RESERVE));

    return (PCALLSTACK) (dwSP - ALIGNSTK (sizeof(CALLSTACK)));
}

static void SetStackOverflow (PEXCEPTION_RECORD pExr)
{
    pExr->ExceptionCode = (DWORD) STATUS_STACK_OVERFLOW;
    pExr->ExceptionFlags = EXCEPTION_NONCONTINUABLE;
}

static BOOL CheckCommitStack (PEXCEPTION_RECORD pExr, DWORD dwAddr, DWORD mode)
{
    LPDWORD pTlsPtr = (KERNEL_MODE == mode)? pCurThread->tlsSecure : pCurThread->tlsNonSecure;
    
    // if we're committing non-secure stack of a untrusted app, check for stack overflow
    if ((dwAddr < pTlsPtr[PRETLS_STACKBOUND])
        && (dwAddr >= pTlsPtr[PRETLS_STACKBASE])) {
        // committed user non-secure stack, check stack overflow
        dwAddr &= -VM_PAGE_SIZE;
        pTlsPtr[PRETLS_STACKBOUND] = dwAddr;
        if (dwAddr < (pTlsPtr[PRETLS_STACKBASE] + MIN_STACK_RESERVE)) {
            // stack overflow
            SetStackOverflow (pExr);
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL IsLastStackPage (DWORD dwAddr, DWORD mode)
{
    const DWORD *pTlsPtr = (KERNEL_MODE == mode)? pCurThread->tlsSecure : pCurThread->tlsNonSecure;
    return (dwAddr - pTlsPtr[PRETLS_STACKBASE]) < VM_PAGE_SIZE;
}

static BOOL SetupContextOnUserStack (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    PTHREAD    pCurTh = pCurThread;
    PCONTEXT   pUsrCtx;
    PEXCEPTION_RECORD pUsrExr;
    // make room for CONTEXT, EXCEPTION_RECORD
    DWORD dwNewSP = (DWORD) CONTEXT_TO_STACK_POINTER(pCtx) - sizeof (CONTEXT) - sizeof (EXCEPTION_RECORD);
    BOOL  fRet = TRUE;

    pUsrCtx = (PCONTEXT) dwNewSP;
    pUsrExr = (PEXCEPTION_RECORD) (pUsrCtx+1);

#ifdef x86
    // x86 needs to push arguments/return address on stack
    dwNewSP -= 4 * sizeof (DWORD);
#endif

    // commit user stack if required. 
    if (!VMProcessPageFault (pVMProc, dwNewSP, TRUE, 0)) {
        SetStackOverflow (pExr);
    } else {
        CheckCommitStack (pExr, dwNewSP, USER_MODE);
    } 

    __try {
        if (dwNewSP < (pCurTh->tlsNonSecure[PRETLS_STACKBASE] + VM_PAGE_SIZE)) {
            // fatal stack overflow
            SetStackOverflow (pExr);
            fRet = FALSE;
        } else {
            memcpy (pUsrCtx, pCtx, sizeof (CONTEXT));                           // copy context
            memcpy (pUsrExr, pExr, sizeof (EXCEPTION_RECORD));                  // copy exception record

#ifdef x86

            ((LPDWORD) dwNewSP)[0] = 0;                                         // return address - any invalid value
            ((LPDWORD) dwNewSP)[1] = (DWORD)pUsrExr;                            // arg0 - pExr
            ((LPDWORD) dwNewSP)[2] = (DWORD)pUsrCtx;                            // arg1 - pCtx
            ((LPDWORD) dwNewSP)[3] = (DWORD)pCurTh->pcstkTop;                   // arg2 - pcstk

#else
            CONTEXT_TO_PARAM_1(pCtx) = (REGTYPE) pUsrExr;
            CONTEXT_TO_PARAM_2(pCtx) = (REGTYPE) pUsrCtx;
            CONTEXT_TO_PARAM_3(pCtx) = (REGTYPE) pCurTh->pcstkTop;
#endif
            CONTEXT_TO_STACK_POINTER (pCtx) = dwNewSP;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // bad user stack, take care of it
        SetStackOverflow (pExr);
        fRet = FALSE;
    }

    DEBUGMSG (ZONE_SEH, (L"Stack Switched, pUsrCtx = %8.8lx pUsrExr = %8.8lx NewSP = %8.8lx, fRet = %d\r\n",
                    pUsrCtx, pUsrExr, dwNewSP, fRet));
    return fRet;
}


DWORD ResetThreadStack (PTHREAD pth)
{
    DWORD dwNewSP;

    pth->tlsNonSecure = TLSPTR (pth->dwOrigBase, pth->dwOrigStkSize);
    dwNewSP = (DWORD) pth->tlsNonSecure - SECURESTK_RESERVE - (VM_PAGE_SIZE >> 1);
    pth->tlsNonSecure[PRETLS_STACKBASE] = pth->dwOrigBase;
    pth->tlsNonSecure[PRETLS_STACKSIZE] = pth->dwOrigStkSize;
    pth->tlsNonSecure[PRETLS_STACKBOUND] = dwNewSP & ~VM_PAGE_OFST_MASK;
    return dwNewSP;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void KPSLExceptionRoutine (DWORD ExceptionFlags, DWORD dwPC)
/*++
Routine Description:
    This function is invoked when an exception occurs in a kernel PSL call which is
    not caught by specific handler. It simply forces an error return from the api
    call.

Arguments:
    ExceptionFlag - specify excepiton flag.

    dwPC - the PC where exception occurs

Return Value:
    An exception disposition value of execute handler is returned.
--*/
{
    extern void SurrenderCritSecs(void);
    RETAILMSG(1, (TEXT("KPSLExceptionHandler: flags=%x ControlPc=%8.8lx\r\n"),
            ExceptionFlags, dwPC));

    SurrenderCritSecs();
}

//------------------------------------------------------------------------------
// Get a copy of the current callstack.  Returns the number of frames if the call 
// succeeded and 0 if it failed (if the given buffer is not large enough to receive
// the entire callstack, and STACKSNAP_FAIL_IF_INCOMPLETE is specified in the flags).
// 
// If STACKSNAP_FAIL_IF_INCOMPLETE is not specified, the call will always fill in
// upto the number of nMax frames.
//------------------------------------------------------------------------------

extern BOOL CanTakeCS (LPCRITICAL_SECTION lpcs);

//
// Try taking ModListcs, return TRUE if we get it. FALSE otherwise
//
BOOL TryLockModuleList ()
{
    extern CRITICAL_SECTION ModListcs;
    volatile CRITICAL_SECTION *vpcs = &ModListcs;
    BOOL fRet = TRUE;

    // if we can take ModListcs right away, take it.
    if ((DWORD) vpcs->OwnerThread == dwCurThId) {
        vpcs->LockCount++; /* We are the owner - increment count */

    } else if (InterlockedTestExchange((LPLONG)&vpcs->OwnerThread,0,dwCurThId) == 0) {
        vpcs->LockCount = 1;

    // ModListcs is owned by other thread, block waiting for it only if
    // CS order isn't violated.
    } else {
        fRet = (!InSysCall () && CanTakeCS (&ModListcs));
        if (fRet) {
            EnterCriticalSection (&ModListcs);
        }
    }
    return fRet;
}

#ifdef x86
// Determines if PC is in prolog
static DWORD IsPCInProlog (DWORD dwPC, BOOL fInKMode)
{
    const BYTE bPrologx86[] = {0x55, 0x8b, 0xec};
    DWORD dwRetVal = 0; // Default to not in prolog

    // if we're not in KMode, validate PC being a user address before reading.
    if (fInKMode || IsValidUsrPtr ((LPVOID) dwPC, sizeof (DWORD), FALSE)) {
        __try {
            if (!memcmp((void*)dwPC, bPrologx86, sizeof(bPrologx86))) { 
                // PC on first instruction (55, push ebp)
                dwRetVal = 1;
            } else if (!memcmp((void*)(dwPC-1), bPrologx86, sizeof(bPrologx86))) { 
                // PC on second instruction (8b ec, mov ebp esp)
                dwRetVal = 2;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwRetVal = 0; // Assume non-Prolog if we blew up trying on access
        }    
    }
    return dwRetVal;
}
#else
__inline BOOL IsAddrInModule (const e32_lite *eptr, DWORD dwAddr)
{
    return (DWORD) (dwAddr - eptr->e32_vbase) < eptr->e32_vsize;
}


BOOL FindPEHeader (PPROCESS pprc, DWORD dwAddr, e32_lite *eptr)
{
    BOOL fRet = FALSE;
    if (dwAddr - (DWORD) pprc->BasePtr < pprc->e32.e32_vsize) {
        fRet = CeSafeCopyMemory (eptr, &pprc->e32, sizeof (e32_lite));
        if (fRet) {
            eptr->e32_vbase = (DWORD) pprc->BasePtr;
        }
        
    } else if (TryLockModuleList ()) {
    
        PMODULE pMod = FindModInProcByAddr (pprc, dwAddr);

        if (pMod) {
            fRet = CeSafeCopyMemory (eptr, &pMod->e32, sizeof (e32_lite));
            if (fRet) {
                eptr->e32_vbase = (DWORD) pMod->BasePtr;
            }
        }
        
        UnlockModuleList ();
    }
    return fRet;
}
#endif

ULONG NKGetThreadCallStack (PTHREAD pth, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip, PCONTEXT pCtx)
{
    ULONG       HighLimit;
    ULONG       LowLimit;
    ULONG       Cnt = 0;
    PTHREAD     pCurTh = pCurThread;
    PPROCESS    pprcOldVM = pCurTh->pprcVM;
    PPROCESS    pprcVM = pVMProc;
    PPROCESS    pprc   = pth->pprcActv;
    PCALLSTACK  pcstk  = pth->pcstkTop;
    LPDWORD     tlsPtr = pth->tlsPtr;
    PPROCESS    pprcOldActv = SwitchActiveProcess (g_pprcNK);
    DWORD       err = 0;
    DWORD       saveKrn;
    LPVOID      pCurFrame = lpFrames;
    ULONG       Frames;
    LPDWORD     pParams;
    DWORD       cbFrameSize = (STACKSNAP_NEW_VM & dwFlags)
                            ? sizeof (CallSnapshot3)
                            : ((STACKSNAP_EXTENDED_INFO & dwFlags)? sizeof (CallSnapshotEx) : sizeof (CallSnapshot));

    dwMaxFrames += dwSkip; // we'll subtract dwSkip when reference lpFrames

    DEBUGCHK (pCurTh->tlsPtr == pCurTh->tlsSecure);
    DEBUGCHK (IsKModeAddr ((DWORD) lpFrames));
    DEBUGCHK (IsKModeAddr ((DWORD) pCtx));
    DEBUGCHK (!pprcOldVM || (pprcOldVM == pprcVM));

    saveKrn = pCurTh->tlsPtr[TLSSLOT_KERNEL];
    pCurTh->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;

    // skip any exception callstack
    if (pcstk && (pcstk->dwPrcInfo & CST_EXCEPTION)) {
        // Fix up the actvproc
        pprc  = pcstk->pprcLast;

        //
        // switch tlsPtr
        //
        if (pcstk->dwPrcInfo & CST_MODE_FROM_USER) {
            tlsPtr = pcstk->pOldTls;
        } else {
            tlsPtr = pth->tlsSecure;
        }

        pcstk = pcstk->pcstkNext;
    }

    if (pCurTh != pth)
    {
        // Not the current thread, we need to switch VM.
        // OK because pprcOldVM is stores the current thread's old VM.
        SwitchVM (pth->pprcVM);
    }

    __try {

        // get stack limits
        if (!GetStackLimitsFromTLS (tlsPtr, &LowLimit, &HighLimit)) {
            err = ERROR_INVALID_PARAMETER;
            
        } else {

#ifdef x86

#define RTN_ADDR(x)                     (*(LPDWORD) ((x) + sizeof(DWORD)))
#define NEXT_FRAME(x)                   (*(LPDWORD) (x))
#define FRAMEPTR_TO_PARAM(fptr, idx)    (*(LPDWORD) ((fptr) + (2+(idx))*sizeof(DWORD)))

            DWORD RtnAddr  = pCtx->Eip;
            DWORD FramePtr = pCtx->Ebp;
            DWORD idxParam;

            DWORD dwSavedEBP = 0;
            DWORD dwInProlog = IsPCInProlog (RtnAddr, tlsPtr == pth->tlsSecure);
            if (1 == dwInProlog) {
                RtnAddr += 3; // Increment by 3 bytes to skip past end of prolog
                dwSavedEBP = pCtx->Ebp;
                FramePtr = pCtx->Esp - 4; // Since ESP hasn't been pushed yet
            } else if (2 == dwInProlog) {
                RtnAddr += 2; // Increment past end of prolog
                FramePtr = pCtx->Esp; // Get it directly since we've already done the push
            }

            for ( ; ; ) {

                if (dwMaxFrames <= Cnt) {
                    if (STACKSNAP_FAIL_IF_INCOMPLETE & dwFlags) {
                        err = ERROR_INSUFFICIENT_BUFFER;
                    }
                    break;
                }

                pParams = NULL;
                if (!(STACKSNAP_INPROC_ONLY & dwFlags) || (pprc == pth->pprcOwner)) {
                    if (Cnt >= dwSkip) {

                        // NOTE: dwReturnAddr is the 1st field on all snapshot format. So
                        //       we can safely do this.
                        ((CallSnapshot *) pCurFrame)->dwReturnAddr = RtnAddr;

                        if ((STACKSNAP_NEW_VM|STACKSNAP_EXTENDED_INFO) & dwFlags) {
                            
                            ((CallSnapshotEx *) pCurFrame)->dwFramePtr = FramePtr;
                            ((CallSnapshotEx *) pCurFrame)->dwCurProc  = pprc->dwId;

                            if (STACKSNAP_NEW_VM & dwFlags) {
                                ((CallSnapshot3 *) pCurFrame)->dwVMProc = pVMProc? pVMProc->dwId : 0;
                                pParams = ((CallSnapshot3 *) pCurFrame)->dwParams;
                            } else {
                                pParams = ((CallSnapshotEx *) pCurFrame)->dwParams;
                            }

                        }
                        
                        pCurFrame = (LPVOID) ((DWORD) pCurFrame + cbFrameSize);
                    }
                } else {
                    // For INPROC stacks, skip frames that are not in the thread's owner process
                    dwSkip++;
                    dwMaxFrames++;
                }

                Cnt++;

                if (   !IsInRange (FramePtr, LowLimit, HighLimit)
                    || !IsDwordAligned (FramePtr)) {
                    break;
                }

                // frame is valid, copy arguments from it
                if (pParams) {
                    for (idxParam = 0; idxParam < 4; idxParam ++) {
                        pParams[idxParam] = FRAMEPTR_TO_PARAM (FramePtr, idxParam);
                    }
                }
                
                RtnAddr = RTN_ADDR (FramePtr);
                if (!RtnAddr) {
                    break;
                }

                if ((SYSCALL_RETURN == RtnAddr)
                    || (DIRECT_RETURN  == RtnAddr)) {

                    if (!pcstk) {
                        err = ERROR_INVALID_PARAMETER;
                        break;
                    }
                    
                    // across PSL boundary
                    RtnAddr = (DWORD) pcstk->retAddr;
                    FramePtr = pcstk->regs[REG_EBP];
                    pprc = pcstk->pprcLast;
                    SwitchVM (pcstk->pprcVM);
                
                    //
                    // switch tlsPtr
                    //
                    if (pcstk->dwPrcInfo & CST_MODE_FROM_USER) {
                        tlsPtr = pcstk->pOldTls;
                    } else {
                        tlsPtr = pth->tlsSecure;
                    }

                    if (!GetStackLimitsFromTLS (tlsPtr, &LowLimit, &HighLimit)) {
                        break;
                    }

                    pcstk = pcstk->pcstkNext;

                } else if (dwSavedEBP) {
                    FramePtr = dwSavedEBP;
                    dwSavedEBP = 0;
                } else {
                    FramePtr = NEXT_FRAME(FramePtr);
                }

            }

#else
            BOOL fNativeFrame;
            DWORD dwCookie = 0; // used by managed unwinder
            LPDWORD pMgdFrameCookie = (dwFlags & STACKSNAP_GET_MANAGED_FRAMES) ? &dwCookie : NULL;
            ULONG    ControlPc = (ULONG) CONTEXT_TO_PROGRAM_COUNTER (pCtx);
            e32_lite e32 = {0};

            // Start with the frame specified by the context record and search
            // backwards through the call frame hierarchy.
            do { 

                if (dwMaxFrames <= Cnt) {
                    if (STACKSNAP_FAIL_IF_INCOMPLETE & dwFlags) {
                        err = ERROR_INSUFFICIENT_BUFFER;
                    }
                    break;
                }

                if (!(STACKSNAP_INPROC_ONLY & dwFlags) || (pprc == pth->pprcOwner)) {
                    if (Cnt >= dwSkip) {



                        // NOTE: dwReturnAddr is the 1st field on all snapshot format. So
                        //       we can safely do this.
                        ((CallSnapshot *) pCurFrame)->dwReturnAddr = ControlPc;

                        if ((STACKSNAP_NEW_VM|STACKSNAP_EXTENDED_INFO) & dwFlags) {
                            
                            ((CallSnapshotEx *) pCurFrame)->dwFramePtr = (ULONG) pCtx->IntSp;
                            ((CallSnapshotEx *) pCurFrame)->dwCurProc  = pprc->dwId;

                            if (STACKSNAP_NEW_VM & dwFlags) {
                                ((CallSnapshot3 *) pCurFrame)->dwVMProc = pVMProc? pVMProc->dwId : 0;
                                pParams = ((CallSnapshot3 *) pCurFrame)->dwParams;
                            } else {
                                pParams = ((CallSnapshotEx *) pCurFrame)->dwParams;
                            }

							// This is kind of meaningless. r0-r3 are not restored during unwind and
                            // could've been used for temporaries in function. 
                            pParams[0] = (DWORD) CONTEXT_TO_PARAM_1(pCtx);
                            pParams[1] = (DWORD) CONTEXT_TO_PARAM_2(pCtx);
                            pParams[2] = (DWORD) CONTEXT_TO_PARAM_3(pCtx);
                            pParams[3] = (DWORD) CONTEXT_TO_PARAM_4(pCtx);
                        }
                        
                        pCurFrame = (LPVOID) ((DWORD) pCurFrame + cbFrameSize);


                    }
                } else {
                    // For INPROC stacks, skip frames that are not in the thread's owner process
                    dwSkip++;
                    dwMaxFrames++;
                }

                fNativeFrame = IsAddrInModule (&e32, ControlPc)
                        || FindPEHeader (IsKModeAddr (ControlPc)? g_pprcNK : pprc, ControlPc, &e32);
                
                if (fNativeFrame || pMgdFrameCookie) {
                    ControlPc = (* g_pfnRtlUnwindOneFrame) (
                                                    ControlPc,
                                                    tlsPtr,
                                                    pCtx,
                                                    &e32,
                                                    fNativeFrame ? NULL : pMgdFrameCookie
                                                    );
                } else {
                    // can't find function pdata, assume LEAF function
                    ControlPc = 0;
                }

                Cnt++;
                if (!ControlPc) {
                    // unwind to a NULL address, assume end of callstack
                    if (Cnt > 1) {
                        break;
                    }
                    ControlPc = (ULONG) pCtx->IntRa - INST_SIZE;
                }

                // Set point at which control left the previous routine.
                if (IsPSLBoundary (ControlPc)) {
                    if (!pcstk) {
                        break;
                    }
                    
                    // across PSL boundary
                    MDRestoreCalleeSavedRegisters (pcstk, pCtx);
                    
                    pCtx->IntSp = pcstk->dwPrevSP;
                    pCtx->IntRa = (ULONG)pcstk->retAddr;
                    ControlPc = (ULONG) pCtx->IntRa - INST_SIZE;
                    
                    pprc = pcstk->pprcLast;
                    SwitchVM (pcstk->pprcVM);
                
                    //
                    // switch tlsPtr
                    //
                    if (pcstk->dwPrcInfo & CST_MODE_FROM_USER) {
                        tlsPtr = pcstk->pOldTls;
                    } else {
                        tlsPtr = pth->tlsSecure;
                    }

                    // invalid stack
                    if (!GetStackLimitsFromTLS (tlsPtr, &LowLimit, &HighLimit)) {
                        break;
                    }

                    pcstk = pcstk->pcstkNext;
                }
            
            } while (((ULONG) pCtx->IntSp < HighLimit) && ((ULONG) pCtx->IntSp > LowLimit));

#endif
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        err = ERROR_INVALID_PARAMETER;
    }

    SwitchActiveProcess (pprcOldActv);
    SwitchVM (pprcVM);
    pCurTh->pprcVM = pprcOldVM;
    pCurTh->tlsPtr[TLSSLOT_KERNEL] = saveKrn;

    if (Cnt < dwSkip) {
        err = ERROR_INVALID_PARAMETER;
        Frames = 0;
    } else {
        Frames = Cnt - dwSkip;
    }

    if ((err) || (STACKSNAP_RETURN_FRAMES_ON_ERROR & dwFlags)) {
        KSetLastError (pCurTh, err);
        if (!(STACKSNAP_RETURN_FRAMES_ON_ERROR & dwFlags))
        {
            Frames = 0;
        }
    }

    return Frames;
}

//
// Find find module and offset given a PC
//
LPCWSTR FindModuleNameAndOffset (DWORD dwPC, LPDWORD pdwOffset)
{
    LPCWSTR pszName = L"???";
    DWORD   dwBase  = 0;

    if ((dwPC - (DWORD) pActvProc->BasePtr) < pActvProc->e32.e32_vsize) {
        pszName = pActvProc->lpszProcName;
        dwBase  = (DWORD) pActvProc->BasePtr;
        
    } else if (TryLockModuleList ()) {
        PMODULE pMod = FindModInProcByAddr (IsKModeAddr (dwPC)? g_pprcNK : pActvProc, dwPC);

        if (pMod) {
            pszName = pMod->lpszModName;
            dwBase  = (DWORD) pMod->BasePtr;
        }
        
        UnlockModuleList ();
    }

    *pdwOffset = dwPC - dwBase;

    return pszName;
}



static BOOL IsBreakPoint (DWORD dwExcpCode)
{
    return ((DWORD) STATUS_BREAKPOINT == dwExcpCode)
        || ((DWORD) STATUS_SINGLE_STEP == dwExcpCode);
}


static DWORD SetupExceptionTerminate (PTHREAD pCurTh, PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    PPROCESS pprc    = pCurTh->pprcOwner;
    PTHREAD  pMainTh = MainThreadOfProcess (pprc);

    DEBUGCHK (IsInKVM ((DWORD) pCurTh->pcstkTop));

    SET_DEAD (pCurTh);
    SET_DYING (pCurTh);         // set DYING bit, such that the dwExitCode argument is ignored in NKExitThread.
    CLEAR_USERBLOCK (pCurTh);
    pCurTh->dwExitCode = pExr->ExceptionCode;
    pCurTh->pprcActv = pCurTh->pprcVM = pprc;
    KCall ((PKFN) SetCPUASID, pCurTh);

    if (pprc != g_pprcNK) {
        pprc->fFlags |= PROC_TERMINATED;
    }

    CONTEXT_TO_PROGRAM_COUNTER (pCtx) = (ULONG) NKExitThread;
    CONTEXT_TO_STACK_POINTER (pCtx)   = (ULONG) pCurTh->pcstkTop;
    
#ifdef ARM
    // run in ARM mode
    pCtx->Psr &= ~EXEC_STATE_MASK;
#endif

    RETAILMSG (1, (L"%s thread in proc %8.8lx faulted, Exception code = %8.8lx, Exception Address = %8.8x!\r\n",
        (pMainTh == pCurTh)? L"Main" : L"Secondary", pprc->dwId, pExr->ExceptionCode, pExr->ExceptionAddress));

    if ((pMainTh != pCurTh) && (pprc != g_pprcNK)) {
        RETAILMSG(1,(L"Terminating process %8.8lx (%s)!\r\n", pprc->dwId, pprc->lpszProcName));
        KillOneThread (pMainTh, pExr->ExceptionCode);
    }

    return KERNEL_MODE;
}

#define EXCEPTION_ID_RAISED_EXCEPTION               ((DWORD) -1)
#define EXCEPTION_ID_SECURE_STACK_OVERFLOW          ((DWORD) -2)
#define EXCEPTION_ID_USER_STACK_OVERFLOW            ((DWORD) -3)

#define EXCEPTION_STRING_RAISED_EXCEPTION           "Raised Exception"
#define EXCEPTION_STRING_SECURE_STACK_OVERFLOW      "Secure Stack Overflow"
#define EXCEPTION_STRING_USER_STACK_OVERFLOW        "User Stack Overflow"
#define EXCEPTION_STRING_UNKNOWN                    "Unknown Exception"

const LPCSTR ppszOsExcp [] = {
        EXCEPTION_STRING_USER_STACK_OVERFLOW,          // ID == -3
        EXCEPTION_STRING_SECURE_STACK_OVERFLOW,        // ID == -2
        EXCEPTION_STRING_RAISED_EXCEPTION,             // ID == -1
};
const DWORD nOsExcps = sizeof(ppszOsExcp)/sizeof(ppszOsExcp[0]);


extern const LPCSTR g_ppszMDExcpId [MD_MAX_EXCP_ID+1];

LPCSTR GetExceptionString (DWORD dwExcpId)
{
    LPCSTR pszExcpStr;
    if (dwExcpId <= MD_MAX_EXCP_ID) {
        pszExcpStr = g_ppszMDExcpId[dwExcpId];
        
    } else {
        dwExcpId += nOsExcps;
        if (dwExcpId < nOsExcps) {
            // original id is -1, -2, or -3
            pszExcpStr = ppszOsExcp[dwExcpId];
            
        } else {
            pszExcpStr = EXCEPTION_STRING_UNKNOWN;
        }
    }

    return pszExcpStr;
}

#ifdef x86
DWORD SafeGetReturnAddress (const CONTEXT *pCtx)
{
    PTHREAD pCurTh = pCurThread;
    LPDWORD pFrame = (LPDWORD) (pCtx->Ebp + 4);
    DWORD   dwRa = 0;
    if (   IsDwordAligned (pFrame)
        && ((DWORD)pFrame > pCtx->Esp)
        && ((KERNEL_MODE == GetContextMode (pCtx)) || IsValidUsrPtr (pFrame, sizeof(DWORD), TRUE))) {
        DWORD saveKrn = pCurThread->tlsPtr[TLSSLOT_KERNEL];
        pCurTh->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;
        CeSafeCopyMemory (&dwRa, pFrame, sizeof (DWORD));
        pCurTh->tlsPtr[TLSSLOT_KERNEL] = saveKrn;
    }
    return dwRa;
}

#else
#define SafeGetReturnAddress(pCtx)      CONTEXT_TO_RETURN_ADDRESS(pCtx)
#endif


//
// print exception message
//
static void PrintException (DWORD dwExcpId, PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    LPCWSTR pszNamePC, pszNameRA;
    DWORD dwPC     = CONTEXT_TO_PROGRAM_COUNTER(pCtx);
    DWORD dwOfstPC = dwPC;
    DWORD dwRA     = SafeGetReturnAddress (pCtx);
    DWORD dwOfstRA = dwRA;
    pszNamePC = FindModuleNameAndOffset (dwOfstPC, &dwOfstPC);
    pszNameRA = FindModuleNameAndOffset (dwOfstRA, &dwOfstRA);
    
    NKDbgPrintfW(L"Exception '%a' (0x%x): Thread-Id=%8.8lx(pth=%8.8lx), Proc-Id=%8.8lx(pprc=%8.8lx) '%s', VM-active=%8.8lx(pprc=%8.8lx) '%s'\r\n",
            GetExceptionString (dwExcpId), 
            (EXCEPTION_ID_RAISED_EXCEPTION == dwExcpId)? pExr->ExceptionCode : dwExcpId, 
            dwCurThId, pCurThread,
            pActvProc->dwId, pActvProc, SafeGetProcName (pActvProc),
            pVMProc->dwId,   pVMProc,   SafeGetProcName (pVMProc)
            );
    NKDbgPrintfW(L"PC=%8.8lx(%s+0x%8.8lx) RA=%8.8lx(%s+0x%8.8lx) SP=%8.8lx, BVA=%8.8lx\r\n", 
            dwPC, pszNamePC, dwOfstPC, 
            dwRA, pszNameRA, dwOfstRA,
            CONTEXT_TO_STACK_POINTER (pCtx),
            pExr->ExceptionInformation[1]);

}


static BOOL NotifyKernelDebugger (
    PEXCEPTION_RECORD pExr,
    PCONTEXT pCtx,
    BOOL     f2ndChance,
    BOOL     fSkipKDbgTrap,
    BOOL     fSkipUserDbgTrap
    ) 
{
    PTHREAD pCurTh = pCurThread;
    DWORD   dwSavedLastErr = KGetLastError (pCurTh);
    BOOL    fRet;

    fRet = (!fSkipUserDbgTrap && UserDbgTrap (pExr, pCtx, f2ndChance))
        || (!fSkipKDbgTrap    && HDException (pExr, pCtx, (BOOLEAN) f2ndChance));
    
    // restore last error
    KSetLastError (pCurTh, dwSavedLastErr);
    return fRet;
}

DWORD UnwindExceptionAcrossPSL (
    PEXCEPTION_RECORD pExr,
    PCONTEXT pCtx,
    PCONTEXT pCtxOrig
)
{
    PTHREAD    pCurTh  = pCurThread;
    PCALLSTACK pcstk   = pCurTh->pcstkTop; 
    const DWORD *lptls = (pcstk->dwPrcInfo & CST_MODE_FROM_USER)? pCurTh->tlsNonSecure : pCurTh->tlsPtr;
    DWORD dwFaultedPC  = CONTEXT_TO_PROGRAM_COUNTER (pCtxOrig);
    DWORD mode;

    DEBUGCHK (pcstk && pcstk->pcstkNext);
    //
    // What we need to do here is to pop off the last callstack, restore all
    // callee saved registers and redo SEH handling as if the exception happens
    // right after returning form PerformCallback
    DEBUGMSG (ZONE_SEH, (L"callstack not empty, walk the callstack, pcstkNext = %8.8lx\r\n", pcstk->pcstkNext));

    pcstk = pcstk->pcstkNext;
    pCurTh->pcstkTop = pcstk;

    NotifyKernelDebugger (pExr, pCtxOrig, TRUE, IsNoFaultSet (lptls), TRUE);
    // We record that we just sent the 2nd chance exception to Watson 
    // to avoid creating an additional dump when we redo SEH handling
    // from the caller boundary
    pCurTh->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_CAPTURED_PSL_CALL;

    CONTEXT_TO_PROGRAM_COUNTER (pCtx) = (DWORD) pcstk->retAddr - INST_SIZE;
    CONTEXT_TO_STACK_POINTER (pCtx)   = pcstk->dwPrevSP;

    MDRestoreCalleeSavedRegisters (pcstk, pCtx);

    // perform kernel clean up if faulted inside kernel
    if (!((CST_UMODE_SERVER|CST_CALLBACK) & pcstk->dwPrcInfo)) {
        KPSLExceptionRoutine (pExr->ExceptionFlags, dwFaultedPC);
    }

    // though we'd like to *not* propagate the "stack overflow" exception across PSL, changing it
    // can cause BC issue (test failed due to error code)
#if 0
    if ((DWORD) STATUS_STACK_OVERFLOW == pExr->ExceptionCode) {
        // stack overflow doesn't propagate across PSL boundary as we're using differnt stacks.
        // use a different exception code for the PSL
        pExr->ExceptionCode = (DWORD) STATUS_MARSHALL_OVERFLOW;
    }
#endif
    UnwindOneCstk (pcstk, TRUE);

    if (pcstk->dwPrcInfo) {
        mode = USER_MODE;
        // clear volatile registers if returning to user mode? might disclose information if we don't
        MDClearVolatileRegs (pCtx);
    } else {
        mode = KERNEL_MODE;
    }

    return mode;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
ExceptionDispatch(
    PEXCEPTION_RECORD pExr,
    PCONTEXT pCtx
    ) 
{
    PTHREAD     pCurTh      = pCurThread;
    PCALLSTACK  pcstkExcp   = pCurTh->pcstkTop;           // exception callstack
    PCALLSTACK  pcstkActv   = pcstkExcp->pcstkNext;       // active callstack
    DWORD       mode        = (pcstkExcp->dwPrcInfo & CST_MODE_FROM_USER)? USER_MODE : KERNEL_MODE;
    BOOL        fHandled    = FALSE;
    BOOL        fRunSEH     = TRUE;
    BOOL        f2ndChance  = FALSE;
    DWORD       dwExcpId    = EXCEPTION_ID_RAISED_EXCEPTION;
    const DWORD *lptls      = (KERNEL_MODE != mode) ? pCurTh->tlsNonSecure : pCurTh->tlsPtr;
    BOOL        fDataBreakpoint = FALSE;
    BOOL        fCaptureFPUContext = (lptls[TLSSLOT_KERNEL] & TLSKERN_FPU_USED) && !(lptls[TLSSLOT_KERNEL] & TLSKERN_IN_FPEMUL);

    DEBUGMSG (ZONE_SEH, (L"", MDDbgPrintExceptionInfo (pExr, pcstkExcp)));
    
    memset (pExr, 0, sizeof(EXCEPTION_RECORD));
    DEBUGCHK (pCurTh->tlsPtr == pCurTh->tlsSecure);

    // Update CONTEXT with infomation saved in the EXCINFO structure
    CONTEXT_TO_PROGRAM_COUNTER (pCtx) = (DWORD) pcstkExcp->retAddr;
    CONTEXT_TO_STACK_POINTER (pCtx)   = pcstkExcp->dwPrevSP;
    pExr->ExceptionAddress            = (PVOID) (DWORD) pcstkExcp->retAddr;

#ifdef ARM
    // we always build with THUMB support. No longer "ifdef THUMBSUPPORT" here
    if (CONTEXT_TO_PROGRAM_COUNTER (pCtx) & 1) {
        pCtx->Psr |= THUMB_STATE;
    }
#endif

    // Check for RaiseException call versus a CPU detected exception.
    // RaiseException just becomes a call to CaptureContext as a KPSL.
    //
    // HandleException leave pprcLast field to be NULL.
    //
    if (pcstkExcp->pprcLast) {

        // From RaiseException. The stack looks like this. The only difference between
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
        //      |    context record     |
        //      |                       |
        //      |                       |
        //      -------------------------
        //      | RA (x86 only) and     |<----- (pExr+1)
        //      |args of RaiseExcepiton |
        //      |                       |
        //      -------------------------
        
        // Fill in exception record information from the parameters passed to
        // the RaiseException call.
        LPDWORD pArgs = MDGetRaisedExceptionInfo (pExr, pCtx, pcstkExcp);

#ifdef x86        
        // Remove return address from the stack
        pCtx->Esp += 4;

        // x86 restore exception pointer
        TLS2PCR (lptls)->ExceptionList = pcstkExcp->regs[REG_EXCPLIST];

#endif
        // add CST_EXCEPTION flag for exception unwinding.
        pcstkExcp->dwPrcInfo |= CST_EXCEPTION;

        // The callee-saved registers are saved inside callstack structure. CaptureContext gets
        // the register contents right after the ObjectCall. We need to restore the callee saved
        // registers or the context will not be correct.
        MDRestoreCalleeSavedRegisters (pcstkExcp, pCtx);
                
        //
        // 2nd chance exception report always give 0 as arg count and lpArguments as ptr to original context
        //
        f2ndChance = (EXCEPTION_FLAG_UNHANDLED == pExr->ExceptionFlags)
                && !pExr->NumberParameters
                && pArgs;

        if (f2ndChance) {

            // 2nd chance exception:
            DEBUGMSG (ZONE_SEH, (L"2nd chance Exception, pcstkActv = %8.8lx\r\n", pcstkActv));

            // copy the original exception information to the exception record
            CeSafeCopyMemory (pExr, ((PCONTEXT)pArgs) + 1, sizeof (EXCEPTION_RECORD));
            pExr->ExceptionFlags = EXCEPTION_NONCONTINUABLE;

            // if we're in callback/API, report the exeption to the caller, as if the exception occurs
            // right at the point of the API call.
            if (pcstkActv) {

                mode = UnwindExceptionAcrossPSL (pExr, pCtx, (PCONTEXT) pArgs);
                pcstkActv = pcstkActv->pcstkNext;
                fHandled = TRUE;

            } else {
#ifdef MIPS
                // MIPS had 4 register save area defined in CONTEXT structure!!!
                DWORD dwOfst = 4 * REG_SIZE;
#else
                DWORD dwOfst = 0;
#endif
                // prepare context for debugger
                CeSafeCopyMemory ((LPBYTE)pCtx+dwOfst, (LPBYTE) pArgs+dwOfst, sizeof (CONTEXT)-dwOfst);

                DEBUGMSG (ZONE_SEH, (L"Unhandled Exception, original (IP = %8.8lx, SP = %8.8lx)\r\n",
                    CONTEXT_TO_PROGRAM_COUNTER(pCtx), CONTEXT_TO_STACK_POINTER (pCtx)));
            }

            fRunSEH = fHandled;  // unhandled 2nd chance exception resumes to exit the thread,
           // while 'handled' 2nd chance exception require SEH dispatch
           // again in PSL's context
             

        }
    } else {

        // hardware exception

        pcstkExcp->pprcLast = pActvProc;   // fix callstack to make it normal

        if (TEST_STACKFAULT (pCurTh)) {
            // secure stack overflow (non-fatal)
            dwExcpId = EXCEPTION_ID_SECURE_STACK_OVERFLOW;
            SetStackOverflow (pExr);
            fHandled = FALSE;
            CLEAR_STACKFAULT (pCurTh);
        } else {
            fHandled = MDHandleHardwareException (pcstkExcp, pExr, pCtx, &dwExcpId);

            if (fHandled && MDIsPageFault (dwExcpId)) {
                // handled pagefault. check stack overflow
                fHandled = CheckCommitStack (pExr, pExr->ExceptionInformation[1], mode);

                if (!fHandled) {
                    // user stack overflow
                    dwExcpId = EXCEPTION_ID_USER_STACK_OVERFLOW;
                }
            }
        }
        fRunSEH = !fHandled;

    }

    DEBUGMSG (ZONE_SEH, (L"fHandled = %d, fRunSEH = %d, f2ndChance = %d, mode = 0x%x, SP = %8.8lx\r\n",
            fHandled, fRunSEH, f2ndChance, mode, CONTEXT_TO_STACK_POINTER (pCtx)));
    
    // update context mode
    SetContextMode (pCtx, mode);

    if(!fHandled
        && (pExr->ExceptionCode == (DWORD) STATUS_ACCESS_VIOLATION)
        && (pExr->NumberParameters >= 2))
    {
        fDataBreakpoint = HDIsDataBreakpoint(pExr->ExceptionInformation[1], pExr->ExceptionInformation[0]);
    }

    // print exception info on 1st chance excepiton
    if (!f2ndChance
        && !fHandled
        && !IsBreakPoint (pExr->ExceptionCode)
        && !fDataBreakpoint
        && !IsNoFaultMsgSet (lptls)) {
        PrintException (dwExcpId, pExr, pCtx);
    }

    // special handling for break point (never call SEH, do 1st and 2nd in a row, always resume)
    if (IsBreakPoint (pExr->ExceptionCode) || fDataBreakpoint) {
        
        BOOL fSkipUserDbgTrap = pvHDNotifyExdi
                && (CONTEXT_TO_PROGRAM_COUNTER(pCtx) >= (DWORD)pvHDNotifyExdi)
                && (CONTEXT_TO_PROGRAM_COUNTER(pCtx) <= ((DWORD)pvHDNotifyExdi + HD_NOTIFY_MARGIN));

        fSkipUserDbgTrap = fSkipUserDbgTrap || fDataBreakpoint;

        fRunSEH = FALSE;

        if (   !NotifyKernelDebugger (pExr, pCtx, FALSE, FALSE, fSkipUserDbgTrap)       // 1st chance
            && !NotifyKernelDebugger (pExr, pCtx, TRUE,  FALSE, fSkipUserDbgTrap)) {    // 2nd chance
            
            if (!fSkipUserDbgTrap) {
                RETAILMSG(1, (TEXT("DEBUG_BREAK @%8.8lx Ignored.\r\n"), CONTEXT_TO_PROGRAM_COUNTER(pCtx)));
            }
            MDSkipBreakPoint (pExr, pCtx);
        }
        
    // normal exception - notify debugger, kill thread on 2nd chance exception (not continue-able)
    } else if (!fHandled) {

        if (NotifyKernelDebugger (pExr, pCtx, f2ndChance, IsNoFaultSet (lptls), FALSE)) {
            // debugger handled the exception
            fRunSEH = f2ndChance = FALSE;
            
        } else if (f2ndChance) {
            // unhandled 2nd chance exception, terminate the process/thread
            
            DEBUGCHK (!fRunSEH && !pcstkActv);

            RETAILMSG(1, (TEXT("\r\nUnhandled exception %8.8lx:\r\n"), pExr->ExceptionCode));

            if (InSysCall()) {
                MDDumpFrame (pCurTh, dwExcpId, pExr, pCtx, 0);
                OEMIoControl (IOCTL_HAL_HALT, NULL, 0, NULL, 0, NULL);
                DEBUGCHK (0);

            } else if (!(pCurTh->tlsSecure[TLSSLOT_KERNEL] & TLSKERN_THRDEXITING)) {
                mode = SetupExceptionTerminate (pCurTh, pExr, pCtx);
                RETAILMSG(1, (TEXT("Terminating thread %8.8lx\r\n"), pCurTh));
                
            } else {
                // faulted inside NKExitThread !!!!
                RETAILMSG (1, (TEXT("Thread %8.8lx faulted inside NKExitThread, sleeping forever\r\n"), pCurTh));
                NKSleep (INFINITE);
                DEBUGCHK (0);
            }
        }
    }

    if (fRunSEH) {

        if (KERNEL_MODE != mode) {
            fRunSEH = IsValidUsrPtr ((LPVOID) (LONG) CONTEXT_TO_STACK_POINTER (pCtx), REGSIZE, TRUE)
                   && ((EXCEPTION_ID_USER_STACK_OVERFLOW != dwExcpId) || !IsLastStackPage (pExr->ExceptionInformation[1], mode))
                   && SetupContextOnUserStack (pExr, pCtx);
        }

        if (!fRunSEH) {
            // fatal user stack error, bypass SEH
            NKDbgPrintfW (L"!!! Committed last page of the stack (0x%8.8lx) or invalid stack pointer (0x%8.8lx), SEH bypassed, thread terminated !!!\r\n",
                    pExr->ExceptionInformation[1], CONTEXT_TO_STACK_POINTER (pCtx));

            if (pcstkActv) {

                // if we're in callback/API, report the exeption to the caller, as if the exception occurs
                // right at the point of the API/callback call.
                //
                mode = UnwindExceptionAcrossPSL (pExr, pCtx, pCtx);
                pcstkActv = pcstkActv->pcstkNext;
                
                fRunSEH = TRUE;
                
            } else {
                // not in PSL context, terminate the thread
                mode = SetupExceptionTerminate (pCurTh, pExr, pCtx);
            }

        }

        if (fRunSEH && fCaptureFPUContext) {
            MDCaptureFPUContext(pCtx);
        }
    }

    SetContextMode (pCtx, mode);

    if (KERNEL_MODE != mode) {

        // handle thread terminate/delay suspend
        if (pActvProc == pCurTh->pprcOwner) {
            for ( ; ; ) {
                if (GET_DYING (pCurTh) && !GET_DEAD (pCurTh)) {
                    // current thread is been terminated

                    DEBUGCHK (!GET_USERBLOCK (pCurTh));

                    if (pcstkActv) {
                        // callstack not empty, just skip the callback by setting return address to SYSCALL_RETURN
                        CONTEXT_TO_PROGRAM_COUNTER (pCtx) = (ULONG) SYSCALL_RETURN;

                    } else {
                        // callstack empty, terminate the thread
                        CONTEXT_TO_PROGRAM_COUNTER (pCtx) = (ULONG) TrapAddrExitThread;
                        SET_DEAD (pCurTh);
                    }
                    // don't allow suspending a thread been terminated, or the thread could've never exited.
                    pCurTh->bPendSusp   = 0;
                    fRunSEH = FALSE;

                    break;
                }

                if (!pCurTh->bPendSusp) {
                    break;
                }

                // delay suspended - suspend now.
                SCHL_SuspendSelfIfNeeded ();
                CLEAR_USERBLOCK (pCurTh);
            }
        }

        // update TLS to user mode
        pCurTh->tlsPtr = pCurTh->tlsNonSecure;
        PcbSetTlsPtr (pCurTh->tlsPtr);
#ifdef x86
        // TLS must be updated before updating fs base. For any context switch
        // will cause fs to reload from TLS
        UpdateRegistrationPtr (TLS2PCR (pCurTh->tlsPtr));
#endif
    }

    // pop off the Exception callstack.
    pCurTh->pcstkTop = pcstkActv;

    if (!fRunSEH) {
#ifdef x86
        pCtx->SegGs = (KERNEL_MODE == mode)? KGDT_KPCB : KGDT_UPCB;
#endif
    
#ifdef ARM
        // THUMB specific
        if (CONTEXT_TO_PROGRAM_COUNTER (pCtx) & 1) {
            pCtx->Psr |= THUMB_STATE;
        }
#endif

        MDCheckInterlockOperation (pCtx);
    }
    return fRunSEH;
}


static BOOL IsPcAtCaptureContext (DWORD dwPC)
{
#ifdef x86
    return dwPC == (DWORD)CaptureContext;
#else
    return dwPC == ((DWORD)CaptureContext+4);
#endif
}


BOOL KC_CommonHandleException (PTHREAD pth, DWORD arg1, DWORD arg2, DWORD arg3)
{
    PCALLSTACK pcstk;
    DWORD dwThrdPC = THRD_CTX_TO_PC (pth);
    DWORD dwThrdSP = (DWORD) THRD_CTX_TO_SP (pth);
    DWORD dwMode = GetThreadMode (pth);
    DWORD dwSpaceLeft;
    DWORD dwRslt = DCMT_OLD;
    PTHREAD pCurTh = pCurThread;

    KCALLPROFON(65);

    pcstk = ReserveCSTK (dwThrdSP, dwMode);

    if (!IsInKVM ((DWORD) pcstk)) {
        RETAILMSG (1, (L"\r\nFaulted in KCall, PC = %8.8lx, SP = %8.8lx, args = %8.8lx %8.8lx %8.8lx!!\r\n",
            dwThrdPC, dwThrdSP, arg1, arg2, arg3));

        RETAILMSG (1, (L"Original Context when thread faulted:\r\n"));
        MDDumpThreadContext (pCurTh, arg1, arg2, arg3, 0);
        RETAILMSG (1, (L"Context when faulted in KCall:\r\n"));
        MDDumpThreadContext (pth, arg1, arg2, arg3, 0);
    }
    DEBUGMSG (ZONE_SCHEDULE, (L"+HandleException: pcstk = %8.8lx\r\n", pcstk));

    if (((DWORD) pcstk & ~VM_PAGE_OFST_MASK) != ((DWORD) (pcstk+1) & ~VM_PAGE_OFST_MASK)) {
        // pcstk cross page boundary, commit the next page
        dwRslt = KC_VMDemandCommit ((DWORD) (pcstk+1));
    }

    if (DCMT_FAILED != dwRslt) {
        dwRslt = KC_VMDemandCommit ((DWORD) pcstk);
    }
    
    // before we touch pcstk, we need to commit stack or we'll fault while
    // accessing it.
    switch (dwRslt) {
    case DCMT_OLD:
        // already commited. continue exception handling
        break;

    case DCMT_NEW:
        // commited a new page. check if we hit the last page.
        // Generate fatal stack overflow exception if yes (won't get into exception handler).

        if (pCurTh->tlsSecure[PRETLS_STACKBOUND] <= (DWORD)pcstk) {
            // within bound, restart the instruction
            KCALLPROFOFF(65);
            DEBUGMSG (ZONE_SCHEDULE, (L"-HandleException 1: committed secure stack page %8.8lx\r\n", (DWORD)pcstk & ~VM_PAGE_OFST_MASK));
            return TRUE; // restart instruction
        }

        // update stack bound
        pCurTh->tlsSecure[PRETLS_STACKBOUND] = (DWORD)pcstk & ~VM_PAGE_OFST_MASK;

        // calculate room left
        dwSpaceLeft = pCurTh->tlsSecure[PRETLS_STACKBOUND] - pCurTh->tlsSecure[PRETLS_STACKBASE];

        if (dwSpaceLeft >= MIN_STACK_RESERVE) {
            // within bound, restart the instruction
            KCALLPROFOFF(65);
            DEBUGMSG (ZONE_SCHEDULE, (L"-HandleException 2: committed secure stack page %8.8lx\r\n", (DWORD)pcstk & ~VM_PAGE_OFST_MASK));
            return TRUE; // restart instruction
        }

        // normal stack overflow exception if still room left
        if (dwSpaceLeft >= VM_PAGE_SIZE) {
            SET_STACKFAULT (pCurTh);
            break;
        }

        // fall through for fatal stack error
        __fallthrough;
        
    default:
    
        // fatal stack error
        NKDbgPrintfW  (L"!FATAL ERROR!: Secure stack overflow - IP = %8.8lx (%8.8lx)\r\n", dwThrdPC, THRD_CTX_TO_PC(pCurTh));
        NKDbgPrintfW  (L"!FATAL ERROR!: Killing thread - pCurThread = %8.8lx\r\n", pCurTh);

        pCurTh->tlsPtr = pCurTh->tlsSecure;
        PcbSetTlsPtr (pCurTh->tlsPtr);
        THRD_CTX_TO_SP (pth) = (DWORD) (pcstk+1);
        THRD_CTX_TO_PARAM_1 (pth) = (DWORD) STATUS_STACK_OVERFLOW;
        THRD_CTX_TO_PARAM_2 (pth) = dwThrdPC;

        // NOTE: fatal stack error can lead to leaks
        pCurTh->pcstkTop = NULL;        // have to discard all the callstack for they're no longer valid
        THRD_CTX_TO_PC (pth) = (DWORD) NKExitThread;

        SET_DEAD (pCurTh);
        CLEAR_USERBLOCK (pCurTh);
        pCurTh->pprcActv = pCurTh->pprcVM = pCurTh->pprcOwner;
        SetCPUASID (pCurTh);

        
        MDDumpThreadContext (pth, arg1, arg2, arg3, 10);
        SetThreadMode (pth, KERNEL_MODE);      // run in kernel mode

        KCALLPROFOFF(65);
        DEBUGMSG (ZONE_SCHEDULE, (L"-HandleException: Fatal stack error pcstk = %8.8lx\r\n", pcstk));
        
        return TRUE;
    }

    // Setup to capture the exception context in kernel mode but
    // running in thread context to allow preemption and stack growth.
    if (!IsPcAtCaptureContext (THRD_CTX_TO_PC (pth)))
    {
        pcstk->dwPrevSP  = dwThrdSP;             // original SP
        pcstk->retAddr   = (RETADDR) dwThrdPC;   // retaddr: IP
        pcstk->pprcLast  = NULL;                 // no process to indicate exception callstack
        pcstk->pprcVM    = pCurTh->pprcVM;       // current VM active
        pcstk->dwPrcInfo = ((USER_MODE == dwMode)? CST_MODE_FROM_USER : 0) | CST_EXCEPTION; // mode
        pcstk->pOldTls   = pCurTh->tlsNonSecure;          // save old TLS
        pcstk->phd       = 0;

        // use the cpu dependent fields to pass extra information to ExceptionDispatch
        MDSetupExcpInfo (pth, pcstk, arg1, arg2, arg3);

        pCurTh->tlsPtr = pCurTh->tlsSecure;
        PcbSetTlsPtr (pCurTh->tlsPtr);
        
        pcstk->pcstkNext = pCurTh->pcstkTop;
        pCurTh->pcstkTop = pcstk;
        THRD_CTX_TO_SP (pth) = (DWORD) pcstk;
        THRD_CTX_TO_PC (pth) = (ulong)CaptureContext;

        KCALLPROFOFF(65);
        DEBUGMSG (ZONE_SCHEDULE, (L"-HandleException: prepare getting into ExceptionDispatch pcstk = %8.8lx\r\n", pcstk));
        return TRUE;            // continue execution
    }

    // recursive exception
    MDDumpThreadContext (pth, arg1, arg2, arg3, 10);
    RETAILMSG(1, (TEXT("Halting thread %8.8lx\r\n"), pCurTh));
    SurrenderCritSecs();
    SCHL_BlockCurThread ();
    KCALLPROFOFF(65);
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void DumpDwords (const DWORD *pdw, int len)
{
    pdw = (const DWORD *) ((DWORD) pdw & ~3);   // dword align pdw
    NKDbgPrintfW(L"\r\nDumping %d dwords at address %8.8lx\r\n", len, pdw);
    for ( ; len > 0; pdw+=4, len -= 4) {
        NKDbgPrintfW(L"%8.8lx - %8.8lx %8.8lx %8.8lx %8.8lx\r\n", pdw, pdw[0], pdw[1], pdw[2], pdw[3]);
    }
    NKDbgPrintfW(L"\r\n");
}



