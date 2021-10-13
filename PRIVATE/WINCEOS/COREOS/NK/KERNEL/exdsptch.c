//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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


// Obtain current stack limits
//  if we're on original stack, or if there is a secure stack (pth->tlsSecure != pth->tlsNonSecure)
//  and we're on the secure stack (pth->tlsPtr == pth->tlsSecure), then *LL == KSTKBASE(pth), 
//  *HL = pth->tlsPtr. Otherwise we're on FIBER stack.
//
void NKGetStackLimits (PTHREAD pth, LPDWORD pLL, LPDWORD pHL)
{
    if (((*pLL = KSTKBASE(pth)) == pth->dwOrigBase)         // on original stack?
        || ((pth->tlsNonSecure != pth->tlsSecure) && (pth->tlsPtr == pth->tlsSecure))) { // on secure stack?
        *pHL = (DWORD) pth->tlsPtr;
    } else {
        *pHL = KPROCSTKSIZE(pth) + *pLL         // fibers have fix size stack
                - cbMDStkAlign;                 // back off a bit or SEH would go across the boundary
    }
}


BOOL ValidateStack (PTHREAD pth, DWORD dwCurSP, BOOL *pfTlsOk)
{
    DWORD lb = KSTKBASE(pth), ub;
    *pfTlsOk = TRUE;

    if (((lb | KSTKBOUND(pth) | KPROCSTKSIZE (pth)) & (PAGE_SIZE-1))
        || (KSTKBOUND(pth) < lb)
        || ((lb >> VA_SECTION) != (DWORD) (pth->pOwnerProc->procnum + 1))) {
        *pfTlsOk = FALSE;
    } else {

        ub = lb + ((lb == pth->dwOrigBase)? pth->dwOrigStkSize : KPROCSTKSIZE (pth));

        if (KSTKBOUND(pth) >= ub) {
            *pfTlsOk = FALSE;
        }    
    }
    
    return *pfTlsOk && (dwCurSP > lb) && (dwCurSP < ub);

}

DWORD FixTlsAndSP (PTHREAD pth, BOOL fFixTls)
{
    PCALLSTACK pcstk;

    if (fFixTls) {
        KSTKBASE(pth) = pth->dwOrigBase;
        KSTKBOUND(pth) = (DWORD) pth->tlsPtr & -PAGE_SIZE;
        KPROCSTKSIZE(pth) = pth->dwOrigStkSize;
        KTHRDINFO(pth) = pth->tlsPtr[PRETLS_CURFIBER] = 0;
        pth->tlsSecure[TLSSLOT_KERNEL] = pth->tlsNonSecure[TLSSLOT_KERNEL] = TLSKERN_TRYINGTODIE;
    }
    // find the point we call into PSL
    for (pcstk = (PCALLSTACK)((ulong)pth->pcstkTop &~1); pcstk; pcstk = (PCALLSTACK)((ulong)pcstk->pcstkNext&~1)) {
        if ((pcstk->dwPrevSP) && (pcstk->pprcLast == pth->pOwnerProc)) {
            return pcstk->dwPrevSP;
        }
    }

    // extra 64 bytes for safe keeping
    return (ulong) pth->tlsPtr - SECURESTK_RESERVE - 64;
}


void SetupCSTK (PCALLSTACK pcstk)
{
    pcstk->dwPrcInfo = CST_CALLBACK | CST_MODE_TO_USER;
    pcstk->akyLast 
        = pcstk->dwPrevSP
        = pcstk->extra = 0;
    pcstk->retAddr = 0; // RA to be fill by assembly
    pcstk->pcstkNext = pCurThread->pcstkTop;
    pcstk->pprcLast = pCurProc;
}



//
// Define local macros.
//
// Raise noncontinuable exception with associated exception record.
//

#define RAISE_EXCEPTION(Status, ExceptionRecordt) DEBUGCHK(0)


#ifdef x86

#define EXCEPTION_CHAIN_END ((PEXCEPTION_REGISTRATION_RECORD)-1)

//
// Define private function prototypes.
//

EXCEPTION_DISPOSITION RtlpExecuteHandlerForException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine,
    IN OUT PCALLSTACK pcstk,
    IN ULONG ExceptionMode);

EXCEPTION_DISPOSITION RtlpExecuteHandlerForUnwind(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine,
    IN OUT PCALLSTACK pcstk,
    IN ULONG ExceptionMode);

BOOLEAN NKUnwind(
    IN PTHREAD pth,
    IN PCALLSTACK TargetCStk OPTIONAL,
    IN PEXCEPTION_REGISTRATION_RECORD TargetFrame OPTIONAL,
    IN ULONG TargetIp OPTIONAL,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN ULONG ReturnValue,
    IN PCONTEXT ContextRecord);

void UpdateASID(IN PTHREAD pth, IN PPROCESS pProc, IN ACCESSKEY aky);

extern EXCEPTION_ROUTINE KPSLExceptionRoutine;




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void __inline 
StoreRegistrationPointer(
    void *RegistrationPointer
    )
{
    __asm {
        mov eax, [RegistrationPointer]
        mov dword ptr fs:[0], eax
    }
}

#pragma warning(disable:4035)



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PVOID __inline 
GetRegistrationHead(void)
{
    __asm mov eax, dword ptr fs:[0];
}
        
#pragma warning(default:4035)





//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOLEAN
NKDispatchException(
    IN PTHREAD pth,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord
    )
{
    EXCEPTION_DISPOSITION Disposition;
    PEXCEPTION_REGISTRATION_RECORD RegistrationPointer;
    PEXCEPTION_REGISTRATION_RECORD NestedRegistration;
    DISPATCHER_CONTEXT DispatcherContext;
    UINT HighAddress;
    UINT HighLimit;
    UINT LowLimit;
    PCALLSTACK pcstk;
    PCALLSTACK TargetCStk;
    PPROCESS pProc;
    PPROCESS pprcSave;
    ACCESSKEY akySave;
    EXCEPTION_REGISTRATION_RECORD TempRegistration;
    CONTEXT ContextRecord1;
    LPDWORD tlsSave;

    // Get current stack limits
    NKGetStackLimits(pth, &LowLimit, &HighLimit);
    // protect ourselves from stack fault while running the handler (assuming we don't need more than one page to run)
    if ((DWORD) &Disposition < (LowLimit + MIN_STACK_RESERVE + PAGE_SIZE)) {
        ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
        ExceptionRecord->ExceptionCode = STATUS_STACK_OVERFLOW;
        DEBUGMSG(1, (TEXT("NKDispatchException: returning failure due to stack overflow\r\n")));
        return FALSE;
    }
    

    DEBUGMSG(ZONE_SEH, (TEXT("X86 DispatchException(%x %x) pc %x code %x address %x\r\n"),
        ExceptionRecord, ContextRecord, ContextRecord->Eip,
        ExceptionRecord->ExceptionCode, ExceptionRecord->ExceptionAddress));
    pcstk = pth->pcstkTop;
    akySave = pth->aky;
    pprcSave = pCurProc;
    tlsSave = pth->tlsPtr;

    memcpy(&ContextRecord1, ContextRecord, sizeof(CONTEXT));

    /*
       Start with the frame specified by the context record and search
       backwards through the call frame hierarchy attempting to find an
       exception handler that will handler the exception.
     */
    RegistrationPointer = GetRegistrationHead();

    if (!RegistrationPointer || (RegistrationPointer == EXCEPTION_CHAIN_END)) {
        // in case there is no exception handler, just return unhandled
        return FALSE;
    }
    NestedRegistration = 0;
    TargetCStk = 0;
    do {
        // Check for PSL call boundary.
        DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: Proc=%8.8lx RP=%8.8lx\r\n"),
                pCurProc, RegistrationPointer));
        while (RegistrationPointer == (PEXCEPTION_REGISTRATION_RECORD)-2) {
            PEXCEPTION_ROUTINE pEH = KPSLExceptionRoutine;

            if (!pcstk) {
                RETAILMSG(1, (TEXT("NKDispatchException: no CALLSTACK with RP=%d\r\n"),
                        RegistrationPointer));
                KPlpvTls = pth->tlsPtr = tlsSave;
                UpdateASID(pth, pprcSave, akySave);
                return FALSE;
            }

            // in a PSL call.
            pProc = pcstk->pprcLast;
            if (!TargetCStk 
                && pcstk->akyLast 
                && ((pcstk->dwPrcInfo & CST_IN_KERNEL) || (pCurProc == pProc) || (pEH = pCurProc->pfnEH) != 0)) {
                DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: Found PSL Global Handler\r\n")));
                TargetCStk = pcstk;
                RegistrationPointer = &TempRegistration;
                RegistrationPointer->Next = (PEXCEPTION_REGISTRATION_RECORD)-2;
                RegistrationPointer->Handler = pEH;
                break;
            } else {
                TargetCStk = 0;
                RegistrationPointer = (PEXCEPTION_REGISTRATION_RECORD)pcstk->extra;
                DEBUGCHK (pcstk->dwPrcInfo & CST_CALLBACK);
                
                DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: Walking across CallBack\r\n")));
                if (pcstk->dwPrevSP) {
                    ContextRecord1.Esp = pcstk->dwPrevSP;
                    KPlpvTls = pth->tlsPtr = pth->tlsSecure;
                    NKGetStackLimits(pth, &LowLimit, &HighLimit);
                }
                
                SetContextMode(&ContextRecord1, (pcstk->dwPrcInfo & CST_MODE_FROM_USER)? USER_MODE : KERNEL_MODE);
                UpdateASID(pth, pProc, pcstk->akyLast ? pcstk->akyLast : pth->aky);
                pcstk = pcstk->pcstkNext;
                DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: PSL return Proc=%8.8lx RP=%8.8lx pcstk=%8.8lx\r\n"),
                        pProc, RegistrationPointer, pcstk));
            }
        }

        DEBUGCHK (RegistrationPointer != (PEXCEPTION_REGISTRATION_RECORD)-2);
        /*
           If the call frame is not within the specified stack limits or the
           call frame is unaligned, then set the stack invalid flag in the
           exception record and return FALSE. Else check to determine if the
           frame has an exception handler.
         */
        HighAddress = (UINT)RegistrationPointer + sizeof(EXCEPTION_REGISTRATION_RECORD);
        if ((UINT)RegistrationPointer < LowLimit
                || HighAddress > HighLimit
                || ((UINT)RegistrationPointer & 0x3) != 0) { /* Not aligned */
            ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
            break;
        } else {
            /* The handler must be executed by calling another routine
             that is written in assembler. This is required because
             up level addressing of the handler information is required
             when a nested exception is encountered.
             */
            PCALLSTACK pcstkForHandler = 0;
            DWORD ctxMode = GetContextMode(&ContextRecord1);
            
            DispatcherContext.ControlPc = 0;
            DispatcherContext.RegistrationPointer = RegistrationPointer;
            DEBUGMSG(ZONE_SEH, (TEXT("Calling exception handler @%8.8lx RP=%8.8lx\r\n"),
                    RegistrationPointer->Handler, RegistrationPointer));

            if (KERNEL_MODE != ctxMode) {
                if (!(pcstkForHandler = (PCALLSTACK) AllocMem (HEAP_CALLSTACK))) {
                    // we're completely out of memory, can't do much about it
                    ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
                    break;
                }
                SetupCSTK (pcstkForHandler);
            }

            Disposition = RtlpExecuteHandlerForException(ExceptionRecord,
                    RegistrationPointer, ContextRecord, &DispatcherContext,
                    RegistrationPointer->Handler, pcstkForHandler, ctxMode);

            // don't need to free pcstkForHandler since it'll be freed at callback return
            
            DEBUGMSG(ZONE_SEH, (TEXT("exception dispostion = %d\r\n"), Disposition));

            /* If the current scan is within a nested context and the frame
               just examined is the end of the context region, then clear
               the nested context frame and the nested exception in the
               exception flags.
             */
            if (NestedRegistration == RegistrationPointer) {
                ExceptionRecord->ExceptionFlags &= (~EXCEPTION_NESTED_CALL);
                NestedRegistration = 0;
            }

            // Case on the handler disposition.
            switch (Disposition) {
                    // The disposition is to execute a frame based handler. The
                    // target handler address is in DispatcherContext.ControlPc.
                    // Reset to the original context and switch to unwind mode.
                case ExceptionExecuteHandler:
                    DEBUGMSG(ZONE_SEH && DispatcherContext.ControlPc,
                            (TEXT("Unwinding to %8.8lx RegistrationPointer=%8.8lx\r\n"),
                            DispatcherContext.ControlPc, RegistrationPointer));
                    DEBUGMSG(ZONE_SEH && !DispatcherContext.ControlPc,
                            (TEXT("Unwinding to %8.8lx RegistrationPointer=%8.8lx Ebx=%8.8lx Esi=%d\r\n"),
                            ContextRecord->Eip, RegistrationPointer,
                            ContextRecord->Ebx, ContextRecord->Esi));

                    if (tlsSave != pth->tlsPtr) {
                        // stack switch occured.
                        StoreRegistrationPointer ((void *) -2);
                        KPlpvTls = pth->tlsPtr = tlsSave;
                        
                    }
                    UpdateASID(pth, pprcSave, akySave);
                    // Repair the CONTEXT for PSL handlers.
                    if (TargetCStk) {
                        ContextRecord->Esp = TargetCStk->ExEsp;
                        ContextRecord->Ebp = TargetCStk->ExEbp;
                        ContextRecord->Ebx = TargetCStk->ExEbx;
                        ContextRecord->Esi = TargetCStk->ExEsi;
                        ContextRecord->Edi = TargetCStk->ExEdi;
                    }
                    return NKUnwind(pth, TargetCStk, RegistrationPointer,
                            DispatcherContext.ControlPc, ExceptionRecord,
                            ExceptionRecord->ExceptionCode, ContextRecord);

                /* The disposition is to continue execution. If the
                   exception is not continuable, then raise the exception
                   EXC_NONCONTINUABLE_EXCEPTION. Otherwise return
                   TRUE. */
            case ExceptionContinueExecution :
                if ((ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) != 0) {
                    RAISE_EXCEPTION(STATUS_INVALID_DISPOSITION, ExceptionRecord);
                    break;
                } else {
                    KPlpvTls = pth->tlsPtr = tlsSave;
                    UpdateASID(pth, pprcSave, akySave);
                    return TRUE;
                }

                /*
                   The disposition is to continue the search. Get next
                   frame address and continue the search.
                 */

            case ExceptionContinueSearch :
                break;

                /*
                   The disposition is nested exception. Set the nested
                   context frame to the establisher frame address and set
                   nested exception in the exception flags.
                 */

            case ExceptionNestedException :
                ExceptionRecord->ExceptionFlags |= EXCEPTION_NESTED_CALL;

                if (DispatcherContext.RegistrationPointer > NestedRegistration){
                    NestedRegistration = DispatcherContext.RegistrationPointer;
                }
                break;

                /*
                   All other disposition values are invalid. Raise
                   invalid disposition exception.
                 */

            default :
                RETAILMSG(1, (TEXT("NKDispatchException: invalid dispositon (%d)\r\n"),
                        Disposition));
                RAISE_EXCEPTION(STATUS_INVALID_DISPOSITION, ExceptionRecord);
                break;
            }

            RegistrationPointer = RegistrationPointer->Next;
        }
    } while (RegistrationPointer && RegistrationPointer != EXCEPTION_CHAIN_END);
    DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: ran out of exceptions pc=%x code=%x\n"),
        ContextRecord1.Eip, ExceptionRecord->ExceptionCode));
    // RAISE_EXCEPTION(STATUS_INVALID_DISPOSITION, ExceptionRecord);
    KPlpvTls = pth->tlsPtr = tlsSave;
    UpdateASID(pth, pprcSave, akySave);
    return FALSE;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOLEAN
NKUnwind(
    IN PTHREAD pth,
    IN PCALLSTACK TargetCStk OPTIONAL,
    IN PEXCEPTION_REGISTRATION_RECORD TargetFrame OPTIONAL,
    IN ULONG TargetIp OPTIONAL,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN ULONG ReturnValue,
    IN PCONTEXT ContextRecord
    )
/*++
Routine Description:
    This function initiates an unwind of procedure call frames. The machine
    state at the time of the call to unwind is captured in a context record
    and the unwinding flag is set in the exception flags of the exception
    record. If the TargetFrame parameter is not specified, then the exit unwind
    flag is also set in the exception flags of the exception record. A backward
    walk through the procedure call frames is then performed to find the target
    of the unwind operation.

    N.B.    The captured context passed to unwinding handlers will not be
            a  completely accurate context set for the 386.  This is because
            there isn't a standard stack frame in which registers are stored.

            Only the integer registers are affected.  The segement and
            control registers (ebp, esp) will have correct values for
            the flat 32 bit environment.

Arguments:
    pth - Supplies a pointer to thread structure

    TargetCStk - Supplies an optional pointer to a PSL callstack to unwind to.
        This parameter is used instead of TargetFrame.

    TargetFrame - Supplies an optional pointer to the call frame that is the
        target of the unwind. If this parameter is not specified, then an exit
        unwind is performed.

    TargetIp - Supplies an optional instruction address that specifies the
        continuation address of the unwind. This address is ignored if the
        target frame parameter is not specified.

    ExceptionRecord - Supplies an optional pointer to an exception record.

    ReturnValue - Supplies a value that is to be placed in the integer
        function return register just before continuing execution.

    ContextRecord - Supplies a pointer to a context record that can be used
        to store context druing the unwind operation.

Return Value:
    TRUE if target frame or callstack reached.
--*/
{
    DISPATCHER_CONTEXT DispatcherContext;
    EXCEPTION_DISPOSITION Disposition;
    PEXCEPTION_REGISTRATION_RECORD RegistrationPointer;
    PEXCEPTION_REGISTRATION_RECORD PriorPointer;
    ULONG ExceptionFlags;
    ULONG HighAddress;
    ULONG HighLimit;
    ULONG LowLimit;
    PCALLSTACK pcstk, pcstkForHandler;
    PPROCESS pProc;
    BOOLEAN continueLoop = TRUE;
    DWORD ctxMode;

    // Get current stack limits.
    NKGetStackLimits(pth, &LowLimit, &HighLimit);
    pcstk = pth->pcstkTop;

    // If the target frame of the unwind is specified, then set EXCEPTION_UNWINDING
    // flag in the exception flags. Otherwise set both EXCEPTION_EXIT_UNWIND and
    // EXCEPTION_UNWINDING flags in the exception flags.
    ExceptionFlags = ExceptionRecord->ExceptionFlags | EXCEPTION_UNWINDING;
    if (!TargetFrame && !TargetCStk)
        ExceptionFlags |= EXCEPTION_EXIT_UNWIND;

    ContextRecord->Eax = ReturnValue;
    if (TargetIp)
        ContextRecord->Eip = TargetIp;


    // Scan backward through the call frame hierarchy, calling exception
    // handlers as they are encountered, until the target frame of the unwind
    // is reached.
    RegistrationPointer = GetRegistrationHead();
    DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: TargetCstk =%8.8lx TargetFrame=%8.8lx, TargetIp = %8.8lx, RP = %8.8lx, ll = %8.8lx, hl = %8.8lx\r\n"),
        TargetCStk, TargetFrame, TargetIp, RegistrationPointer, LowLimit, HighLimit));

    while (RegistrationPointer && RegistrationPointer != EXCEPTION_CHAIN_END) {
        DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: Proc=%8.8lx(%d) RP=%8.8lx\r\n"),
                pCurProc, pCurProc->procnum, RegistrationPointer));

        // Check for PSL call boundary.
        while (RegistrationPointer == (PEXCEPTION_REGISTRATION_RECORD)-2) {
            if (!pcstk) {
                RETAILMSG(1, (TEXT("NKUnwind: no CALLSTACK with RP=%d\r\n"),
                        RegistrationPointer));
                goto Unwind_exit;
            }
            if (pcstk == TargetCStk) {
                DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: TargetCStk reached.\r\n")));
                goto Unwind_exit;
            }

            // in a PSL call.
            RegistrationPointer = (PEXCEPTION_REGISTRATION_RECORD)pcstk->extra;
            if (pcstk->dwPrevSP) {
                ContextRecord->Esp = pcstk->dwPrevSP;
                KPlpvTls = pth->tlsPtr = pth->tlsSecure;
                NKGetStackLimits(pth, &LowLimit, &HighLimit);
                DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: Walking across call stack, ll = %8.8lx, hl = %8.8lx\r\n"), LowLimit, HighLimit));
            }
            pProc = pcstk->pprcLast;
            SetContextMode(ContextRecord, (pcstk->dwPrcInfo & CST_MODE_FROM_USER)? USER_MODE : KERNEL_MODE);
            UpdateASID(pth, pProc, pcstk->akyLast ? pcstk->akyLast : pth->aky);

            pth->pcstkTop = pcstk->pcstkNext;
            if (IsValidKPtr(pcstk))
                FreeMem(pcstk,HEAP_CALLSTACK);

            pcstk = pth->pcstkTop;

            // Update the FS:[0] exception chain
            StoreRegistrationPointer(RegistrationPointer);
            DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: PSL return Proc=%8.8lx RP=%8.8lx pcstk=%8.8lx\r\n"),
                    pProc, RegistrationPointer, pcstk));
        }


        // If the call frame is not within the specified stack limits or the
        // call frame is unaligned, then raise the exception STATUS_BAD_STACK.
        // Else restore the state from the specified frame to the context
        // record.
        HighAddress = (ULONG)RegistrationPointer + sizeof(EXCEPTION_REGISTRATION_RECORD);
        if ((ULONG)RegistrationPointer < LowLimit
                || HighAddress > HighLimit
                || ((ULONG)RegistrationPointer & 0x3) != 0) {
            RAISE_EXCEPTION(STATUS_BAD_STACK, ExceptionRecord);
            break;
        }

        do {
            if (RegistrationPointer == TargetFrame){
                // Establisher frame is the target of the unwind
                // operation.  Set the target unwind flag.  We still
                // need to excute the loop body one more time, so we
                // can't just break out.
                ExceptionFlags |= EXCEPTION_TARGET_UNWIND;
                continueLoop = FALSE;
            }
            // Update the flags.
            ExceptionRecord->ExceptionFlags = ExceptionFlags;
            ExceptionFlags &= ~(EXCEPTION_COLLIDED_UNWIND |
                                EXCEPTION_TARGET_UNWIND);

            // The handler must be executed by calling another routine
            // that is written in assembler. This is required because
            // up level addressing of the handler information is required
            // when a collided unwind is encountered.
            DEBUGMSG(ZONE_SEH, (TEXT("Calling termination handler @%8.8lx RP=%8.8lx\r\n"),
                         RegistrationPointer->Handler, RegistrationPointer));
            pcstkForHandler = 0;
            ctxMode = GetContextMode(ContextRecord);
            if (KERNEL_MODE != ctxMode) {
                if (!(pcstkForHandler = (PCALLSTACK) AllocMem (HEAP_CALLSTACK))) {
                    // we're completely out of memory, can't do much about it
                    ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
                    break;
                }
                SetupCSTK (pcstkForHandler);
            }
            Disposition = RtlpExecuteHandlerForUnwind(ExceptionRecord,
                (PVOID)RegistrationPointer, ContextRecord, (PVOID)&DispatcherContext,
                RegistrationPointer->Handler, pcstkForHandler, ctxMode);
            DEBUGMSG(ZONE_SEH, (TEXT("  disposition = %d\r\n"), Disposition));
            // don't need to free pcstkForHandler since it'll be freed at callback return

            if (Disposition == ExceptionContinueSearch) {
                DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: disposition == ExceptionContinueSearch\r\n")));
                break;
            }

            if (Disposition != ExceptionCollidedUnwind) {
                RETAILMSG(1, (TEXT("NKUnwind: invalid dispositon (%d)\r\n"),
                              Disposition));
                RAISE_EXCEPTION(STATUS_INVALID_DISPOSITION, ExceptionRecord);
            }
                
            DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: disposition == ExceptionCollidedUnwind\r\n")));
            ExceptionFlags |= EXCEPTION_COLLIDED_UNWIND;
            if (RegistrationPointer == DispatcherContext.RegistrationPointer) {
                break;
            }

            // Pick up the registration pointer that was active at
            // the time of the unwind, and continue.
            RegistrationPointer = DispatcherContext.RegistrationPointer;

        } while (continueLoop);

        // If this is the target of the unwind, then continue execution
        // by calling the continue system service.
        if (RegistrationPointer == TargetFrame) {
            StoreRegistrationPointer(RegistrationPointer);
            break;
        }

        // Step to next registration record and unlink
        // the unwind handler since it has been called.
        PriorPointer = RegistrationPointer;
        RegistrationPointer = RegistrationPointer->Next;
        StoreRegistrationPointer(RegistrationPointer);

        











    }

Unwind_exit:
    // If the registration pointer is equal to the target frame
    // pointer, then continue execution. Otherwise, an exit unwind was
    // performed or the target of the unwind did not exist and the
    // debugger and subsystem are given a second chance to handle the
    // unwind.
    if ((TargetFrame && RegistrationPointer == TargetFrame)
            || (TargetCStk && pcstk == TargetCStk)) {
        DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: continuing @%8.8lx\r\n"),
                CONTEXT_TO_PROGRAM_COUNTER(ContextRecord)));
        return TRUE;
    }
    return FALSE;
}
#endif

#ifndef x86

extern void MD_CBRtn (void);
extern void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx);

#if MIPS
#define COMBINED_PDATA           // define to enable combined pdata format

#define INST_SIZE   4
#define STK_ALIGN   0x7
#define RetValue    IntV0

#define RESTORE_EXTRA_INFO(Context, extra) ((Context)->IntGp = (extra))
#define FIXTOC(Context)
#endif

#if SHx
#define COMPRESSED_PDATA        // define to enable compressed pdata format

#define INST_SIZE   2
#define STK_ALIGN   0x3
#define IntRa       PR
#define IntSp       R15
#define RetValue    R0

#define RESTORE_EXTRA_INFO(Context, extra)
#define FIXTOC(Context)
#endif

#if ARM
#define COMPRESSED_PDATA        // define to enable compressed pdata format

#define INST_SIZE   4
#define STK_ALIGN   0x3
#define IntRa       Lr
#define IntSp       Sp
#define RetValue    R0
#define RESTORE_EXTRA_INFO(Context, extra) ((Context)->R9 = (extra))
#define FIXTOC(Context)

#endif


#ifdef INST_SIZE
//
// Define private function prototypes.
//

ULONG
RtlpVirtualUnwind(
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN PCONTEXT ContextRecord,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame);

EXCEPTION_DISPOSITION RtlpExecuteHandlerForException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN ULONG EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine,
    IN OUT PCALLSTACK pcstk,
    IN ULONG ExceptionMode);

EXCEPTION_DISPOSITION RtlpExecuteHandlerForUnwind(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN ULONG EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine,
    IN OUT PCALLSTACK pcstk);

BOOLEAN NKUnwind(
    IN PTHREAD pth,
    IN PCALLSTACK TargetCStk OPTIONAL,
    IN ULONG TargetFrame OPTIONAL,
    IN ULONG TargetIp OPTIONAL,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN ULONG ReturnValue,
    IN PCONTEXT ContextRecord,
    IN PCALLSTACK NewStkTop);

PRUNTIME_FUNCTION NKLookupFunctionEntry(
    IN PPROCESS pProc,
    IN ULONG ControlPc,
    PRUNTIME_FUNCTION prf);

void UpdateASID(IN PTHREAD pth, IN PPROCESS pProc, IN ACCESSKEY aky);

extern EXCEPTION_ROUTINE KPSLExceptionRoutine;

#if defined(COMPRESSED_PDATA) || defined(COMBINED_PDATA)
typedef struct PDATA {
    ULONG pFuncStart;
    ULONG PrologLen : 8;        // low order bits
    ULONG FuncLen   : 22;       // high order bits
    ULONG ThirtyTwoBits : 1;    // 0/1 == 16/32 bit instructions
    ULONG ExceptionFlag : 1;    // highest bit
} PDATA, *PPDATA;
#endif

typedef struct PDATA_EH {
    PEXCEPTION_ROUTINE pHandler;
    PVOID pHandlerData;
} PDATA_EH, *PPDATA_EH;

#if defined(COMBINED_PDATA)
typedef struct COMBINED_HEADER {
    DWORD zero1;               // must be zero if header present
    DWORD zero2;               // must be zero if header present
    DWORD Uncompressed;        // count of uncompressed entries
    DWORD Compressed;          // count of compressed entries
} COMBINED_HEADER, *PCOMBINED_HEADER;
#endif



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOLEAN
NKDispatchException(
    IN PTHREAD pth,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord
    )
/*++
Routine Description:
    This function attempts to dispatch an exception to a frame based
    handler by searching backwards through the stack based call frames.
    The search begins with the frame specified in the context record and
    continues backward until either a handler is found that handles the
    exception, the stack is found to be invalid (i.e., out of limits or
    unaligned), or the end of the call hierarchy is reached.

    As each frame is encounter, the PC where control left the corresponding
    function is determined and used to lookup exception handler information
    in the runtime function table built by the linker. If the respective
    routine has an exception handler, then the handler is called. If the
    handler does not handle the exception, then the prologue of the routine
    is executed backwards to "unwind" the effect of the prologue and then
    the next frame is examined.

Arguments:
    pth - Supplies a pointer to thread structure
    ExceptionRecord - Supplies a pointer to an exception record.
    ContextRecord - Supplies a pointer to a context record.

Return Value:
    If the exception is handled by one of the frame based handlers, then
    a value of TRUE is returned. Otherwise a value of FALSE is returned.
--*/
{
    CONTEXT ContextRecord1;
    CONTEXT LocalContext;
    PCONTEXT OriginalContext;
    ULONG ControlPc;
    DISPATCHER_CONTEXT DispatcherContext;
    EXCEPTION_DISPOSITION Disposition;
    ULONG EstablisherFrame;
    ULONG ExceptionFlags;
    PRUNTIME_FUNCTION FunctionEntry;
    BOOLEAN InFunction;
    BOOLEAN Handled;
    ULONG HighLimit;
    ULONG LowLimit;
    ULONG NestedFrame;
    ULONG NextPc;
    ULONG PrevSp;
    PCALLSTACK pcstk, pcstkForHandler;
    PCALLSTACK TargetCStk;
    PPROCESS pprcSave;
    ACCESSKEY akySave;
    RUNTIME_FUNCTION TempFunctionEntry;
    LPDWORD tlsSave;

    // Get current stack limits, copy the context record, get the initial
    // PC value, capture the exception flags, and set the nested exception
    // frame pointer.
    NKGetStackLimits(pth, &LowLimit, &HighLimit);

    // protect ourselves from stack fault while running the handler (assuming we don't need more than one page to run)
    if ((DWORD) &ContextRecord1 < (LowLimit + MIN_STACK_RESERVE + PAGE_SIZE)) {
        ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
        ExceptionRecord->ExceptionCode = STATUS_STACK_OVERFLOW;
        DEBUGMSG(1, (TEXT("NKDispatchException: returning failure due to stack overflow\r\n")));
        return FALSE;
    }
    
    //
    // Redirect ContextRecord to a local copy of the incoming context. The
    // exception dispatch process modifies the PC of this context. Unwinding
    // of nested exceptions will fail when unwinding through CaptureContext
    // with a modified PC.
    //

    OriginalContext = ContextRecord;
    ContextRecord = &LocalContext;
    memcpy(ContextRecord, OriginalContext, sizeof(CONTEXT));

    memcpy(&ContextRecord1, ContextRecord, sizeof(CONTEXT));
    ControlPc = CONTEXT_TO_PROGRAM_COUNTER(&ContextRecord1);
    ExceptionFlags = ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE;
    NestedFrame = 0;

    pcstk = pth->pcstkTop;

    akySave = pth->aky;
    pprcSave = pCurProc;
    tlsSave = pth->tlsPtr;

    // Start with the frame specified by the context record and search
    // backwards through the call frame hierarchy attempting to find an
    // exception handler that will handle the exception.
    do { 
        // Lookup the function table entry using the point at which control
        // left the procedure.
        DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException(%08x): Proc=%8.8lx ControlPc=%8.8lx SP=%8.8lx, PSR = %8.8lx\r\n"),
                ContextRecord, pCurProc, ControlPc, ContextRecord1.IntSp, ContextRecord1.Psr));
        PrevSp = (ULONG) ContextRecord1.IntSp;
        TargetCStk = 0;
        ControlPc = ZeroPtr(ControlPc);
        FunctionEntry = NKLookupFunctionEntry(pCurProc, ControlPc, &TempFunctionEntry);

        // If there is a function table entry for the routine, then virtually
        // unwind to the caller of the current routine to obtain the virtual
        // frame pointer of the establisher and check if there is an exception
        // handler for the frame.
        if (FunctionEntry != NULL) {
            NextPc = RtlVirtualUnwind(ControlPc,
                                      FunctionEntry,
                                      &ContextRecord1,
                                      &InFunction,
                                      &EstablisherFrame);

            // If the virtual frame pointer is not within the specified stack
            // limits or the virtual frame pointer is unaligned, then set the
            // stack invalid flag in the exception record and return exception
            // not handled. Otherwise, check if the current routine has an
            // exception handler.
            if ((EstablisherFrame < LowLimit) 
                    || (EstablisherFrame > HighLimit)
                    || ((EstablisherFrame & STK_ALIGN) != 0)) {
                DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: Bad stack frame=%8.8lx Low=%8.8lx High=%8.8lx\r\n"),
                    EstablisherFrame, LowLimit, HighLimit));
                ExceptionFlags |= EXCEPTION_STACK_INVALID;
                break;

            } else if ((FunctionEntry->ExceptionHandler != NULL) && InFunction) {
                // The frame has an exception handler. The handler must be
                // executed by calling another routine that is written in
                // assembler. This is required because up level addressing
                // of the handler information is required when a nested
                // exception is encountered.
invokeHandler:
                DispatcherContext.ControlPc = ControlPc;
                DispatcherContext.FunctionEntry = FunctionEntry;
                DispatcherContext.EstablisherFrame = EstablisherFrame;
                DispatcherContext.ContextRecord = ContextRecord;

                ExceptionRecord->ExceptionFlags = ExceptionFlags;
                DEBUGMSG(ZONE_SEH, (TEXT("Calling exception handler @%8.8lx(%8.8lx) Frame=%8.8lx\r\n"),
                        FunctionEntry->ExceptionHandler, FunctionEntry->HandlerData,
                        EstablisherFrame));
                pcstkForHandler = 0;
                if (KERNEL_MODE != GetContextMode (&ContextRecord1)) {
                    if (!(pcstkForHandler = (PCALLSTACK) AllocMem (HEAP_CALLSTACK))) {
                        // we're completely out of memory, can't do much about it
                        ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
                        break;
                    }
                    SetupCSTK (pcstkForHandler);
                }
                Disposition = RtlpExecuteHandlerForException(ExceptionRecord,
                        EstablisherFrame, ContextRecord, &DispatcherContext,
                        FunctionEntry->ExceptionHandler, pcstkForHandler, ContextRecord1.Psr);
                // don't need to free pcstkForHandler since it'll be freed at callback return
                ExceptionFlags |=
                    (ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE);
                DEBUGMSG(ZONE_SEH, (TEXT("exception dispostion = %d\r\n"), Disposition));

                // If the current scan is within a nested context and the frame
                // just examined is the end of the nested region, then clear
                // the nested context frame and the nested exception flag in
                // the exception flags.
                if (NestedFrame == EstablisherFrame) {
                    ExceptionFlags &= (~EXCEPTION_NESTED_CALL);
                    NestedFrame = 0;
                }

                // Case on the handler disposition.
                switch (Disposition) {
                    // The disposition is to execute a frame based handler. The
                    // target handler address is in DispatcherContext.ControlPc.
                    // Reset to the original context and switch to unwind mode.
                case ExceptionExecuteHandler:
                    DEBUGMSG(ZONE_SEH, (TEXT("Unwinding to %8.8lx Frame=%8.8lx\r\n"),
                            DispatcherContext.ControlPc, EstablisherFrame));
                    KPlpvTls = pth->tlsPtr = tlsSave;
                    UpdateASID(pth, pprcSave, akySave);
                    Handled =  NKUnwind(pth, TargetCStk, EstablisherFrame,
                                DispatcherContext.ControlPc, ExceptionRecord,
                                ExceptionRecord->ExceptionCode, ContextRecord, pcstk);

                    memcpy(OriginalContext, ContextRecord, sizeof(CONTEXT));
                    return Handled;

                    // The disposition is to continue execution.
                    // If the exception is not continuable, then raise the
                    // exception STATUS_NONCONTINUABLE_EXCEPTION. Otherwise
                    // return exception handled.
                case ExceptionContinueExecution :
                    DEBUGMSG(ZONE_SEH, (TEXT("Continuing execution ...\r\n")));
                    if ((ExceptionFlags & EXCEPTION_NONCONTINUABLE) != 0)
                        RAISE_EXCEPTION(STATUS_NONCONTINUABLE_EXCEPTION, ExceptionRecord);
                    KPlpvTls = pth->tlsPtr = tlsSave;
                    UpdateASID(pth, pprcSave, akySave);

                    memcpy(OriginalContext, ContextRecord, sizeof(CONTEXT));
                    return TRUE;

                    // The disposition is to continue the search.
                    // Get next frame address and continue the search.
                case ExceptionContinueSearch :
                    break;

                    // The disposition is nested exception.
                    // Set the nested context frame to the establisher frame
                    // address and set the nested exception flag in the
                    // exception flags.
                case ExceptionNestedException :
                    DEBUGMSG(ZONE_SEH, (TEXT("Nested exception: frame=%8.8lx\r\n"), 
                            DispatcherContext.EstablisherFrame));
                    ExceptionFlags |= EXCEPTION_NESTED_CALL;
                    if (DispatcherContext.EstablisherFrame > NestedFrame)
                        NestedFrame = DispatcherContext.EstablisherFrame;
                    break;

                    // All other disposition values are invalid.
                    // Raise invalid disposition exception.
                default :
                    RETAILMSG(1, (TEXT("NKDispatchException: invalid dispositon (%d)\r\n"),
                            Disposition));
                    RAISE_EXCEPTION(STATUS_INVALID_DISPOSITION, ExceptionRecord);
                }
            } else {
                if (NextPc != ControlPc)
                    PrevSp = 0; // don't terminate loop because sp didn't change.
            }
                    
        } else {
            // Set point at which control left the previous routine.
            NextPc = (ULONG) ContextRecord1.IntRa - INST_SIZE;
            PrevSp = 0; // don't terminate loop because sp didn't change.

            // If the next control PC is the same as the old control PC, then
            // the function table is not correctly formed.
            if (NextPc == ControlPc)
                break;
        }

        // Set point at which control left the previous routine.
        ControlPc = NextPc;
        if (((ControlPc == SYSCALL_RETURN)
                || (ControlPc+INST_SIZE == SYSCALL_RETURN)
                || (ControlPc == DIRECT_RETURN)
                || (ControlPc+INST_SIZE == (DWORD) MD_CBRtn)
                || (ControlPc == (DWORD) MD_CBRtn)
                || (ControlPc+INST_SIZE == DIRECT_RETURN))) {
        
            PPROCESS pProc; 
            PEXCEPTION_ROUTINE pEH; 

            if (!pcstk) {
                RETAILMSG(1, (TEXT("NKDispatchException: no CALLSTACK object to continue...\r\n")));
                KPlpvTls = pth->tlsPtr = tlsSave;
                UpdateASID(pth, pprcSave, akySave);
                return FALSE;
            }

            // Server IPC return. If the server process has registered an
            // exception handler, the call that handler before unwinding
            // in the calling process.
            pProc = pcstk->pprcLast;
            pEH = KPSLExceptionRoutine;

            if (TargetCStk == 0 && pcstk->akyLast && ((pcstk->dwPrcInfo & CST_IN_KERNEL) || (pEH = pCurProc->pfnEH) != 0)) {
                TargetCStk = pcstk;
                DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: Invoke Handler, dwPrevSP = %8.8lx, dwPrcInfo = %8.8lx\r\n"), 
                    pcstk->dwPrevSP, pcstk->dwPrcInfo));
                FunctionEntry = &TempFunctionEntry;
                FunctionEntry->ExceptionHandler = pEH;
                FunctionEntry->BeginAddress = 0;
                FunctionEntry->EndAddress = 0;
                FunctionEntry->HandlerData = 0;
                EstablisherFrame = 0;
                goto invokeHandler;
            }

            // returning from callback  (ASSUMING ALL PSL HAS A HANDLER)
            // If the following debugchk occurs, the PSL doesn't have
            // a default handler and should provide it.
            DEBUGCHK (pcstk->dwPrcInfo & CST_CALLBACK);
            if (pcstk->dwPrevSP) {
                DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: switch to secure stack\r\n")));
                ContextRecord1.IntSp   = EstablisherFrame = pcstk->dwPrevSP + CALLEE_SAVED_REGS;
                MDRestoreCalleeSavedRegisters (pcstk, &ContextRecord1);
                KPlpvTls = pth->tlsPtr = pth->tlsSecure;
                NKGetStackLimits(pth, &LowLimit, &HighLimit);
                PrevSp = 0;
            }

            // Update the return address and process context
            // information from the thread's call stack list.
            ContextRecord1.IntRa = (ULONG)pcstk->retAddr;
            RESTORE_EXTRA_INFO(&ContextRecord1, pcstk->extra);
            SetContextMode(&ContextRecord1, (pcstk->dwPrcInfo & CST_MODE_FROM_USER)? USER_MODE : KERNEL_MODE);
            UpdateASID(pth, pProc, pcstk->akyLast ? pcstk->akyLast : pth->aky);
            ControlPc = (ULONG) ContextRecord1.IntRa - INST_SIZE;
            DEBUGMSG(ZONE_SEH, (TEXT("NKDispatchException: callback ret=%8.8lx\r\n"),
                    ContextRecord1.IntRa));
            pcstk = pcstk->pcstkNext;
        }
    } while ((ULONG) ContextRecord1.IntSp < HighLimit && (ULONG) ContextRecord1.IntSp > PrevSp);

    // Set final exception flags and return exception not handled.
    DEBUGMSG(1, (TEXT("NKDispatchException: returning failure. Flags=%x\r\n"),
            ExceptionFlags));
    KPlpvTls = pth->tlsPtr = tlsSave;
    UpdateASID(pth, pprcSave, akySave);
    ExceptionRecord->ExceptionFlags = ExceptionFlags;

    memcpy(OriginalContext, ContextRecord, sizeof(CONTEXT));
    return FALSE;
}


#if defined(COMPRESSED_PDATA)


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PRUNTIME_FUNCTION
NKLookupFunctionEntry(
    IN PPROCESS pProc,
    IN ULONG ControlPc,
    OUT PRUNTIME_FUNCTION prf
    )
/*++
Routine Description:
    This function searches the currently active function tables for an entry
    that corresponds to the specified PC value.

Arguments:
    pProc - process context for ControlPC

    ControlPc - Supplies the address of an instruction within the specified
        function.

    prf - ptr to RUNTIME_FUNCTION structure to be filled in & returned.

Return Value:
    If there is no entry in the function table for the specified PC, then
    NULL is returned. Otherwise, the address of the function table entry
    that corresponds to the specified PC is returned.
--*/
{
    PPDATA table;
    PPDATA entry;
    PPDATA_EH peh;
    ULONG SizeOfExceptionTable;
    ULONG InstSize;
    LONG High;
    LONG Low;
    LONG Middle;

    __try {
        // Search for the image that includes the specified PC value and locate
        // its function table.
        table = PDataFromPC(ControlPc, pProc, &SizeOfExceptionTable);

        // If a function table is located, then search the function table
        // for a function table entry for the specified PC.
        if (table != NULL) {
            // Initialize search indicies.
            Low = 0;
            High = (SizeOfExceptionTable / sizeof(PDATA)) - 1;

            // Perform binary search on the function table for a function table
            // entry that subsumes the specified PC.
            while (High >= Low) {
                // Compute next probe index and test entry. If the specified PC
                // is greater than of equal to the beginning address and less
                // than the ending address of the function table entry, then
                // return the address of the function table entry. Otherwise,
                // continue the search.
                Middle = (Low + High) >> 1;
                entry = &table[Middle];
                if (ControlPc < entry->pFuncStart) {
                    High = Middle - 1;

                } else if (Middle != High && ControlPc >= (entry+1)->pFuncStart) {
                    Low = Middle + 1;

                } else {
                    // The ControlPc is between the middle entry and the middle+1 entry
                    // or this is the last entry that will be examined. Check ControlPc
                    // against the function length.
                    InstSize = entry->ThirtyTwoBits ? 4 : 2;
                    prf->BeginAddress = entry->pFuncStart;
                    prf->EndAddress = entry->pFuncStart + entry->FuncLen*InstSize;
                    prf->PrologEndAddress = entry->pFuncStart + entry->PrologLen*InstSize;
                    if (ControlPc >= prf->EndAddress)
                        break;  // Not a match, stop searching

                    // Fill in the remaining fields in the RUNTIME_FUNCTION structure.
                    if (entry->ExceptionFlag) {
                        peh = (PPDATA_EH)(entry->pFuncStart & ~(InstSize-1))-1;
                        prf->ExceptionHandler = peh->pHandler;
                        prf->HandlerData = peh->pHandlerData;
                    } else {
                        prf->ExceptionHandler = 0;
                        prf->HandlerData = 0;
                    }

                    return prf;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    // A function table entry for the specified PC was not found.
    return NULL;
}
#endif // COMPRESSED_PDATA

#if defined(COMBINED_PDATA)


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PRUNTIME_FUNCTION
NKLookupFunctionEntry(
    IN PPROCESS pProc,
    IN ULONG ControlPc,
    OUT PRUNTIME_FUNCTION prf
    )
/*++
Routine Description:
    This function searches the currently active function tables for an entry
    that corresponds to the specified PC value.

Arguments:
    pProc - process context for ControlPC

    ControlPc - Supplies the address of an instruction within the specified
        function.

Return Value:
    If there is no entry in the function table for the specified PC, then
    NULL is returned. Otherwise, the address of the function table entry
    that corresponds to the specified PC is returned.
--*/
{
    PCOMBINED_HEADER Header;
    PRUNTIME_FUNCTION OldEntry;
    PRUNTIME_FUNCTION OldTable;
    PPDATA NewEntry;
    PPDATA NewTable;
    PPDATA_EH NewEH;
    ULONG TotalSize;
    ULONG EndAddress;
    LONG NewHigh;
    LONG OldHigh;
    LONG Low;
    LONG Middle;
    UINT InstrShift;

    __try {
        // Search for the image that includes the specified PC value and locate
        // its function table.
        Header = PDataFromPC(ControlPc, pProc, &TotalSize);

        // If the exception table doesn't exist or the size is invalid, just return
        if (Header == NULL || TotalSize < sizeof (COMBINED_HEADER)) {
            return NULL;
        }

        // If the PDATA is all old style then the header will actually be the
        // first uncompressed entry and the zero fields will be non-zero
        if (Header->zero1 || Header->zero2) {
            OldTable = (PRUNTIME_FUNCTION)Header;
            OldHigh = (TotalSize / sizeof(RUNTIME_FUNCTION)) - 1;
            NewTable = NULL;
        } else {
            OldTable = (PRUNTIME_FUNCTION)(Header+1);
            OldHigh  = Header->Uncompressed - 1;
            NewTable = (PPDATA)(&OldTable[Header->Uncompressed]);
            NewHigh  = Header->Compressed - 1;
        }

        // Initialize search indicies.
        Low = 0;
                    
        // Perform binary search on the function table for a function table
        // entry that subsumes the specified PC.
        while (OldHigh >= Low) {

            // Compute next probe index and test entry. If the specified PC
            // is greater than of equal to the beginning address and less
            // than the ending address of the function table entry, then
            // return the address of the function table entry. Otherwise,
            // continue the search.
            Middle = (Low + OldHigh) >> 1;
            OldEntry = &OldTable[Middle];

            if (ControlPc < OldEntry->BeginAddress) {
                OldHigh = Middle - 1;

            } else if (ControlPc >= OldEntry->EndAddress) {
                Low = Middle + 1;

            } else {

                // The capability exists for more than one function entry
                // to map to the same function. This permits a function to
                // have discontiguous code segments described by separate
                // function table entries. If the ending prologue address
                // is not within the limits of the begining and ending
                // address of the function able entry, then the prologue
                // ending address is the address of a function table entry
                // that accurately describes the ending prologue address.
                if ((OldEntry->PrologEndAddress < OldEntry->BeginAddress) ||
                    (OldEntry->PrologEndAddress >= OldEntry->EndAddress)) {
                    OldEntry = (PRUNTIME_FUNCTION)OldEntry->PrologEndAddress;
                }
                return OldEntry;
            }
        }

        // If a new function table is located, then search the function table
        // for a function table entry for the specified PC.
        if (NewTable != NULL) {
            // Initialize search indicies.
            Low = 0;

            // Perform binary search on the function table for a function table
            // entry that subsumes the specified PC.
            while (NewHigh >= Low) {

                // Compute next probe index and test entry. If the specified PC
                // is greater than of equal to the beginning address and less
                // than the ending address of the function table entry, then
                // return the address of the function table entry. Otherwise,
                // continue the search.
                Middle = (Low + NewHigh) >> 1;
                NewEntry = &NewTable[Middle];

                InstrShift = 1 + NewEntry->ThirtyTwoBits;
                EndAddress = NewEntry->pFuncStart + (NewEntry->FuncLen << InstrShift);

                if (ControlPc < NewEntry->pFuncStart) {
                    NewHigh = Middle - 1;

                } else if (ControlPc >= EndAddress) {
                    Low = Middle + 1;

                } else {
                    prf->BeginAddress = NewEntry->pFuncStart;
                    prf->EndAddress = EndAddress;
                    prf->PrologEndAddress = NewEntry->pFuncStart + (NewEntry->PrologLen << InstrShift);

                    if (NewEntry->ExceptionFlag) {
                        NewEH = (PPDATA_EH)NewEntry->pFuncStart - 1;
                        prf->ExceptionHandler = NewEH->pHandler;
                        prf->HandlerData = NewEH->pHandlerData;
                    } else {
                        prf->ExceptionHandler = 0;
                        prf->HandlerData = 0;
                    }

                    return prf;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    // A function table entry for the specified PC was not found.
    return NULL;
}
#endif // COMBINED_PDATA



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOLEAN
NKUnwind(
    IN PTHREAD pth,
    IN PCALLSTACK TargetCStk OPTIONAL,
    IN ULONG TargetFrame OPTIONAL,
    IN ULONG TargetIp OPTIONAL,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN ULONG ReturnValue,
    IN PCONTEXT ContextRecord,
    IN PCALLSTACK NewStkTop    
    )
/*++
Routine Description:
    This function initiates an unwind of procedure call frames. The machine
    state at the time of the call to unwind is captured in a context record
    and the unwinding flag is set in the exception flags of the exception
    record. If the TargetFrame parameter is not specified, then the exit unwind
    flag is also set in the exception flags of the exception record. A backward
    scan through the procedure call frames is then performed to find the target
    of the unwind operation.

    As each frame is encounter, the PC where control left the corresponding
    function is determined and used to lookup exception handler information
    in the runtime function table built by the linker. If the respective
    routine has an exception handler, then the handler is called.

Arguments:
    pth - Supplies a pointer to thread structure

    TargetCStk - Supplies an optional pointer to a PSL callstack to unwind to.
        This parameter is used instead of TargetFrame.

    TargetFrame - Supplies an optional pointer to the call frame that is the
        target of the unwind. If this parameter is not specified, then an exit
        unwind is performed.

    TargetIp - Supplies an optional instruction address that specifies the
        continuation address of the unwind. This address is ignored if the
        target frame parameter is not specified.

    ExceptionRecord - Supplies an optional pointer to an exception record.

    ReturnValue - Supplies a value that is to be placed in the integer
        function return register just before continuing execution.

    ContextRecord - Supplies a pointer to a context record that can be used
        to store context druing the unwind operation.

Return Value:
    None.
--*/
{
    ULONG ControlPc;
    DISPATCHER_CONTEXT DispatcherContext;
    PCONTEXT OriginalContext;
    EXCEPTION_DISPOSITION Disposition;
    ULONG EstablisherFrame;
    ULONG ExceptionFlags;
    PRUNTIME_FUNCTION FunctionEntry;
    BOOLEAN InFunction;
    ULONG HighLimit;
    ULONG LowLimit;
    ULONG NextPc;
    PCALLSTACK pcstk, pcstkForHandler;
    RUNTIME_FUNCTION TempFunctionEntry;
    PPROCESS pProc = pCurProc;

    // Get current stack limits, capture the current context, virtually
    // unwind to the caller of this routine, get the initial PC value, and
    // set the unwind target address.
    NKGetStackLimits(pth, &LowLimit, &HighLimit);
    pcstk = pth->pcstkTop;
    ControlPc = CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
    CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) = (ULONG)TargetIp;
    EstablisherFrame = (ULONG) ContextRecord->IntSp;
    OriginalContext = ContextRecord;

    // If the target frame of the unwind is specified, then a normal unwind
    // is being performed. Otherwise, an exit unwind is being performed.


    DEBUGMSG (ZONE_SEH, (L"pcstk = %8.8lx, NewStkTop = %8.8lx, TargetCStk = %8.8lx\n", pcstk, NewStkTop, TargetCStk));

    ExceptionFlags = EXCEPTION_UNWINDING;

    if (!TargetFrame && !TargetCStk)
        ExceptionFlags |= EXCEPTION_EXIT_UNWIND;

    // Scan backward through the call frame hierarchy and call exception
    // handlers until the target frame of the unwind is reached.
    do {
        DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind(%08X): Proc=%8.8lx(%d) ControlPc=%8.8lx SP=%8.8lx\r\n"),
                ContextRecord, pProc, pProc->procnum, ControlPc, ContextRecord->IntSp));
        if (   (ControlPc == SYSCALL_RETURN)
            || (ControlPc+INST_SIZE == SYSCALL_RETURN)
            || (ControlPc+INST_SIZE == (DWORD) MD_CBRtn)
            || (ControlPc == (DWORD) MD_CBRtn)
            || (ControlPc == DIRECT_RETURN)
            || (ControlPc+INST_SIZE == DIRECT_RETURN)) {
            // Server IPC return. If doing a PSL unwind, and this is the
            // target process, then terminate to unwind loop.
            pProc = pcstk->pprcLast;
            if (pcstk == TargetCStk) {
                DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: TargetCStk reached.\r\n")));
                break;
            }

            // Update the return address and process context
            // information from the thread's call stack list.
            ContextRecord->IntRa = (ULONG)pcstk->retAddr;
            RESTORE_EXTRA_INFO(ContextRecord, pcstk->extra);

            // must be calling from KMODE
            DEBUGCHK (!(pcstk->dwPrcInfo & CST_MODE_FROM_USER));
            SetContextMode(ContextRecord, KERNEL_MODE);

            /* Returning from a user mode server, restore the previous
             * address space information and return.
             */
            UpdateASID(pth, pProc, pcstk->akyLast ? pcstk->akyLast : pth->aky);
            ControlPc = (ULONG) ContextRecord->IntRa - INST_SIZE;           

            // We can't unlink CALLSTACK Object here, because C++ Nested
            // exception need callstack for dispatch
#if 0
            // Unlink the most recent CALLSTACK object
            pth->pcstkTop = pcstk->pcstkNext;
#endif
            if (pcstk->dwPrevSP) {
                DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: switch stack, new tls = %8.8lx\r\n"), pth->tlsSecure));
                // update hi/lo/current and targetframe
                KPlpvTls = pth->tlsPtr = pth->tlsSecure;
                ContextRecord->IntSp   = EstablisherFrame = pcstk->dwPrevSP + CALLEE_SAVED_REGS;
                MDRestoreCalleeSavedRegisters (pcstk, ContextRecord);
                NKGetStackLimits(pth, &LowLimit, &HighLimit);
            }
            
#if 0
            if (IsValidKPtr(pcstk))
                FreeMem(pcstk,HEAP_CALLSTACK);
#endif
            pcstk = pcstk->pcstkNext;
            DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: PSL ret=%8.8lx\r\n"),
                    ControlPc + INST_SIZE));
        }
        // Lookup the function table entry using the point at which control
        // left the procedure.
        ControlPc = ZeroPtr(ControlPc);
        FunctionEntry = NKLookupFunctionEntry(pCurProc, ControlPc, &TempFunctionEntry);

        // If there is a function table entry for the routine, then virtually
        // unwind to the caller of the routine to obtain the virtual frame
        // pointer of the establisher, but don't update the context record.
        if (FunctionEntry != NULL) {
            NextPc = RtlpVirtualUnwind(ControlPc,
                                       FunctionEntry,
                                       ContextRecord,
                                       &InFunction,
                                       &EstablisherFrame);

            // If the virtual frame pointer is not within the specified stack
            // limits, the virtual frame pointer is unaligned, or the target
            // frame is below the virtual frame and an exit unwind is not being
            // performed, then raise the exception STATUS_BAD_STACK. Otherwise,
            // check to determine if the current routine has an exception
            // handler.
            if ((EstablisherFrame < LowLimit)
                    || (EstablisherFrame > HighLimit)
                    || (TargetFrame && (TargetFrame < EstablisherFrame))
                    || ((EstablisherFrame & STK_ALIGN) != 0)) {
                RAISE_EXCEPTION(STATUS_BAD_STACK, ExceptionRecord);

            } else if ((FunctionEntry->ExceptionHandler != NULL) && InFunction) {
                // The frame has an exception handler.
                // The control PC, establisher frame pointer, the address
                // of the function table entry, and the address of the
                // context record are all stored in the dispatcher context.
                // This information is used by the unwind linkage routine
                // and can be used by the exception handler itself.
                //
                // A linkage routine written in assembler is used to actually
                // call the actual exception handler. This is required by the
                // exception handler that is associated with the linkage
                // routine so it can have access to two sets of dispatcher
                // context when it is called.
                DispatcherContext.ControlPc = ControlPc;
                DispatcherContext.FunctionEntry = FunctionEntry;
                DispatcherContext.EstablisherFrame = EstablisherFrame;
                DispatcherContext.ContextRecord = ContextRecord;

                // Call the exception handler.
                do {
                    // If the establisher frame is the target of the unwind
                    // operation, then set the target unwind flag.
                    if ((ULONG)TargetFrame == EstablisherFrame)
                        ExceptionFlags |= EXCEPTION_TARGET_UNWIND;
                    ExceptionRecord->ExceptionFlags = ExceptionFlags;

                    // Set the specified return value in case the exception
                    // handler directly continues execution.
                    ContextRecord->RetValue = (ULONG)ReturnValue;
                    DEBUGMSG(ZONE_SEH, (TEXT("Calling unwind handler @%8.8lx Frame=%8.8lx\r\n"),
                            FunctionEntry->ExceptionHandler, EstablisherFrame));

                    pcstkForHandler = 0;
                    if (KERNEL_MODE != GetContextMode (ContextRecord)) {
                        if (!(pcstkForHandler = (PCALLSTACK) AllocMem (HEAP_CALLSTACK))) {
                            // we're completely out of memory, can't do much about it
                            ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
                            break;
                        }
                        SetupCSTK (pcstkForHandler);
                    }
                    Disposition = RtlpExecuteHandlerForUnwind(ExceptionRecord,
                                                    EstablisherFrame,
                                                    ContextRecord,
                                                    &DispatcherContext,
                                                    FunctionEntry->ExceptionHandler,
                                                    pcstkForHandler);

                    // don't need to free pcstkForHandler since it'll be freed at callback return
                    DEBUGMSG(ZONE_SEH, (TEXT("  disposition = %d\r\n"), Disposition));

                    // Clear target unwind and collided unwind flags.
                    ExceptionFlags &= ~(EXCEPTION_COLLIDED_UNWIND |
                                                        EXCEPTION_TARGET_UNWIND);

                    // Case on the handler disposition.
                    switch (Disposition) {
                        // The disposition is to continue the search.
                        //
                        // If the target frame has not been reached, then
                        // virtually unwind to the caller of the current
                        // routine, update the context record, and continue
                        // the search for a handler.
                    case ExceptionContinueSearch :
                        if (EstablisherFrame != (ULONG)TargetFrame) {
                            DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: Calling RtlVirtualUnwind %8.8lx, &8.8lx, %8.8lx\r\n"),
                                ControlPc, FunctionEntry, EstablisherFrame));
                            NextPc = RtlVirtualUnwind(ControlPc,
                                                      FunctionEntry,
                                                      ContextRecord,
                                                      &InFunction,
                                                      &EstablisherFrame);
                        }
                        break;

                        // The disposition is collided unwind.
                        //
                        // Set the target of the current unwind to the context
                        // record of the previous unwind, and reexecute the
                        // exception handler from the collided frame with the
                        // collided unwind flag set in the exception record.
                    case ExceptionCollidedUnwind :
                        ControlPc = DispatcherContext.ControlPc;
                        FunctionEntry = DispatcherContext.FunctionEntry;
                        ContextRecord = DispatcherContext.ContextRecord;
                        CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) = (ULONG)TargetIp;
                        ExceptionFlags |= EXCEPTION_COLLIDED_UNWIND;
                        EstablisherFrame = DispatcherContext.EstablisherFrame;
                        break;

                        // All other disposition values are invalid.
                        // Raise invalid disposition exception.
                    default :
                        RAISE_EXCEPTION(STATUS_INVALID_DISPOSITION, ExceptionRecord);
                    }
                } while ((ExceptionFlags & EXCEPTION_COLLIDED_UNWIND) != 0);

                DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: NextPc =  %8.8lx\r\n"), NextPc));
            } else {
                // If the target frame has not been reached, then virtually unwind to the
                // caller of the current routine and update the context record.
                if (EstablisherFrame != (ULONG)TargetFrame) {
                    NextPc = RtlVirtualUnwind(ControlPc,
                                              FunctionEntry,
                                              ContextRecord,
                                              &InFunction,
                                              &EstablisherFrame);
                }
            }
        } else {
            // Set point at which control left the previous routine.
            NextPc = (ULONG) ContextRecord->IntRa - INST_SIZE;

            // If the next control PC is the same as the old control PC, then
            // the function table is not correctly formed.
            if (NextPc == ControlPc)
                break;  ///RtlRaiseStatus(STATUS_BAD_FUNCTION_TABLE);
        }

        // Set point at which control left the previous routine.
        //
        // N.B. Make sure the address is in the delay slot of the jal
        //      to prevent the boundary condition of the return address
        //      being at the front of a try body.
        ControlPc = NextPc;
        
    } while ((EstablisherFrame < HighLimit) &&
            (EstablisherFrame != (ULONG)TargetFrame));

    // If the establisher stack pointer is equal to the target frame
    // pointer, then continue execution. Otherwise, an exit unwind was
    // performed or the target of the unwind did not exist and the
    // debugger and subsystem are given a second chance to handle the
    // unwind.
    if ((TargetFrame && EstablisherFrame == (ULONG)TargetFrame)
            || (TargetCStk && pcstk == TargetCStk)) {

        if ( ContextRecord != OriginalContext ){        // collided unwind
            //
            //  A collided unwind ocurred and unwinding proceeded
            //  with the collided context. Copy the collided context
            //  back into the original context structure
            //
            memcpy(OriginalContext, ContextRecord, sizeof(CONTEXT));
            ContextRecord = OriginalContext;
        }

        //
        // Free intermediate Kptrs(if present) and unlink intermediate
        // Callstack object
        //
        {
            PCALLSTACK pcstk = pth->pcstkTop;
            PCALLSTACK pcstkNext;

            while (pcstk && (pcstk != NewStkTop)) {
                pcstkNext = pcstk->pcstkNext;
                if (IsValidKPtr(pcstk)) {
                    FreeMem(pcstk, HEAP_CALLSTACK);
                }
                pcstk = pcstkNext;
            }

            // Unlink CallStack
            // pcstk should be NewStkTop
            DEBUGCHK(pcstk == NewStkTop);
            pth->pcstkTop = pcstk;
        }

        FIXTOC(ContextRecord);
        ContextRecord->RetValue = (ULONG)ReturnValue;
        DEBUGMSG(ZONE_SEH, (TEXT("NKUnwind: continuing @%8.8lx, pcstktop = %8.8lx\r\n"),
                CONTEXT_TO_PROGRAM_COUNTER(ContextRecord), pth->pcstkTop));
        return TRUE;
    } else {
        return FALSE;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
ULONG
RtlpVirtualUnwind(
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN PCONTEXT ContextRecord,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame
    )

/*++

Routine Description:

    This function virtually unwinds the specfified function by executing its
    prologue code backwards.

    If the function is a leaf function, then the address where control left
    the previous frame is obtained from the context record. If the function
    is a nested function, but not an exception or interrupt frame, then the
    prologue code is executed backwards and the address where control left
    the previous frame is obtained from the updated context record.

    Otherwise, an exception or interrupt entry to the system is being unwound
    and a specially coded prologue restores the return address twice. Once
    from the fault instruction address and once from the saved return address
    register. The first restore is returned as the function value and the
    second restore is place in the updated context record.

    If a context pointers record is specified, then the address where each
    nonvolatile registers is restored from is recorded in the appropriate
    element of the context pointers record.

    N.B. This function copies the specified context record and only computes
         the establisher frame and whether control is actually in a function.

Arguments:

    ControlPc - Supplies the address where control left the specified
        function.

    FunctionEntry - Supplies the address of the function table entry for the
        specified function.

    ContextRecord - Supplies the address of a context record.

    InFunction - Supplies a pointer to a variable that receives whether the
        control PC is within the current function.

    EstablisherFrame - Supplies a pointer to a variable that receives the
        the establisher frame pointer value.

Return Value:

    The address where control left the previous frame is returned as the
    function value.

--*/
{
    CONTEXT LocalContext;

    //
    // Copy the context record so updates will not be reflected in the
    // original copy and then virtually unwind to the caller of the
    // specified control point.
    //

    memcpy(&LocalContext, ContextRecord, sizeof(CONTEXT));
    return RtlVirtualUnwind(ControlPc,
                            FunctionEntry,
                            &LocalContext,
                            InFunction,
                            EstablisherFrame);
}

#endif  // defined INST_SIZE
#endif // !x86



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
UpdateASID(
    IN PTHREAD pth,
    IN PPROCESS pProc,
    IN ACCESSKEY aky
    )
/*++
Routine Description:
    This function updates the thread's current process and access key information
    from the context struture.

Arguments:
    pth - pointer to thread structure
    pctx - pointer to context record

Return Value:
    None.
--*/
{
    if (pth->pProc != pProc || pth->aky != aky) {
        pth->pProc = pProc;
        pth->aky = aky;
        SetCPUASID(pth);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
long 
KPSLCleanUp(void)
/*++
Routine Description:
    This function is invoked to clean up from a kernel PSL call which has faulted. It
    sets an error code and returns FALSE from the api call.

Arguments:
    none

Return Value:
    FALSE
--*/
{
    KSetLastError(pCurThread,ERROR_INVALID_ADDRESS);
    return 0;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
EXCEPTION_DISPOSITION 
KPSLExceptionRoutine(
    struct _EXCEPTION_RECORD *ExceptionRecord,
    void *EstablisherFrame,
    struct _CONTEXT *ContextRecord,
    struct _DISPATCHER_CONTEXT *DispatcherContext
    )
/*++
Routine Description:
    This function is invoked when an exception occurs in a kernel PSL call which is
    not caught by specific handler. It simply forces an error return from the api
    call.

Arguments:
    ExceptionRecord - Supplies a pointer to an exception record.

    EstablisherFrame - Supplies a pointer to frame of the establisher function.

    ContextRecord - Supplies a pointer to a context record.

    DispatcherContext - Supplies a pointer to the exception dispatcher or
        unwind dispatcher context.

Return Value:
    An exception disposition value of execute handler is returned.
--*/
{
    extern void SurrenderCritSecs(void);
    RETAILMSG(1, (TEXT("KPSLExceptionHandler: flags=%x ControlPc=%8.8lx\r\n"),
            ExceptionRecord->ExceptionFlags, DispatcherContext->ControlPc));

    SurrenderCritSecs();
    // If an unwind is not in progress, then return the address of the psl cleanup
    // routine in DispatcherContext->ControlPc.
    if (IS_DISPATCHING(ExceptionRecord->ExceptionFlags)
            && ExceptionRecord->ExceptionCode != STATUS_INVALID_SYSTEM_SERVICE
            && ExceptionRecord->ExceptionCode != ERROR_INVALID_PARAMETER) {
        DispatcherContext->ControlPc = (DWORD)KPSLCleanUp;
        return ExceptionExecuteHandler;
    }

    // Continue search for exception or termination handlers.
    return ExceptionContinueSearch;
}

//------------------------------------------------------------------------------
// Get a copy of the current callstack.  Returns the number of frames if the call 
// succeeded and 0 if it failed (if the given buffer is not large enough to receive
// the entire callstack, and STACKSNAP_FAIL_IF_INCOMPLETE is specified in the flags).
// 
// If STACKSNAP_FAIL_IF_INCOMPLETE is not specified, the call will always fill in
// upto the number of nMax frames.
//------------------------------------------------------------------------------
#ifdef x86
    #define SKIP_FRAMES 0
#else
    #define SKIP_FRAMES 2
#endif

#define MAX_SKIP        0x10000     // skipping 64K frames? I don't think so

extern BOOL CanTakeCS (LPCRITICAL_SECTION lpcs);

ULONG NKGetCallStackSnapshot (ULONG dwMaxFrames, CallSnapshot lpFrames[], DWORD dwFlags, DWORD dwSkip, PCONTEXT pCtx)
{
    ULONG       HighLimit;
    ULONG       LowLimit;
    ULONG       Cnt = 0;
    PTHREAD     pth = pCurThread;
    PPROCESS    pprcSave = pCurProc;
    PCALLSTACK  pcstk = pth->pcstkTop;
    ACCESSKEY   akySave = pth->aky;
    LPDWORD     tlsSave = pth->tlsPtr;
    DWORD       err = 0;
    DWORD       saveKrnSecure, saveKrnNonSecure;

    dwMaxFrames += dwSkip; // we'll subtract dwSkip when reference lpFrames

    // get stack limit and save TLS settings
    NKGetStackLimits(pCurThread, &LowLimit, &HighLimit);
    saveKrnSecure = pCurThread->tlsSecure[TLSSLOT_KERNEL];
    saveKrnNonSecure = pCurThread->tlsNonSecure[TLSSLOT_KERNEL];
    pCurThread->tlsSecure[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;
    pCurThread->tlsNonSecure[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;

    __try {
#ifdef x86

#define RTN_ADDR(x)     (*(LPDWORD) ((x) + sizeof(DWORD)))
#define NEXT_FRAME(x)   (*(LPDWORD) (x))

        DWORD       FramePtr = pCtx->Ebp;
        DWORD       RtnAddr;
        extern DWORD  MD_CBRtn;

        for (Cnt = 0; Cnt < dwMaxFrames; Cnt ++) {

            if ((FramePtr < LowLimit)
                || (FramePtr > HighLimit)
                || ((FramePtr & 3) != 0)) {
                break;
            }
            if (!(RtnAddr = RTN_ADDR(FramePtr))) {
                // reaches the end
                break;
            }
            if ((SYSCALL_RETURN == RtnAddr)
                || (DIRECT_RETURN  == RtnAddr)
                || ((DWORD) MD_CBRtn == RtnAddr)) {

                if (!pcstk) {
                    err = ERROR_INVALID_PARAMETER;
                    break;
                }
                
                // across PSL boundary
                RtnAddr = (DWORD) pcstk->retAddr;
                FramePtr = pcstk->ExEbp;
                if (pcstk->dwPrevSP) {
					//
					// Cannot switch stack while getting callstack, or try-except will not work.
					// implication: non-trusted apps won't be able to get callstack post PSL boundary
					//
					break;
                    // switch stack (by switch tlsPtr)
                    pth->tlsPtr = (pth->tlsPtr == pth->tlsSecure)? pth->tlsNonSecure : pth->tlsSecure;
                    NKGetStackLimits(pth, &LowLimit, &HighLimit);
                }
                UpdateASID(pth, pcstk->pprcLast, pcstk->akyLast ? pcstk->akyLast : pth->aky);
                pcstk = pcstk->pcstkNext;

            } else { 

                // check for direct PSL calls in KMode                
                if (pcstk && !pcstk->retAddr
                    && (FramePtr > (DWORD) pcstk) && (NEXT_FRAME(FramePtr) <= (DWORD) pcstk)) {
                    UpdateASID(pth, pcstk->pprcLast, pcstk->akyLast ? pcstk->akyLast : pth->aky);
                    pcstk = pcstk->pcstkNext;
                }
                
                FramePtr = NEXT_FRAME(FramePtr);
            }
            if (Cnt >= dwSkip)
                lpFrames[Cnt-dwSkip].dwReturnAddr = (DWORD)MapPtrProc (RtnAddr, pCurProc);

        }
        
#else
        ULONG       ControlPc;
        ULONG       CurrFrame, PrevFrame = LowLimit;
        BOOLEAN     InFunction;
        ULONG       NestedFrame = 0;
        extern CRITICAL_SECTION ModListcs;
        
        // we need ModListcs when getting pDATA. However, it might causes CS order violation
        // and deadlock. So we'll check to see if it's okay to take ModListcs before preceeding.
        if (!CanTakeCS (&ModListcs)) {
            KSetLastError (pth, ERROR_ACCESS_DENIED);
            return 0;
        }
        
        // Start with the frame specified by the context record and search
        // backwards through the call frame hierarchy attempting to find an
        // exception handler that will handle the exception.
        ControlPc = (DWORD) pCtx->IntRa;

        do { 
            PRUNTIME_FUNCTION FunctionEntry;
            RUNTIME_FUNCTION TempFunctionEntry;

            if (dwMaxFrames <= Cnt) {
                if (STACKSNAP_FAIL_IF_INCOMPLETE & dwFlags) {
                    err = ERROR_INSUFFICIENT_BUFFER;
                }
                break;
            }
            ControlPc = ZeroPtr(ControlPc);

            if (!(FunctionEntry = NKLookupFunctionEntry(pCurProc, ControlPc, &TempFunctionEntry))) {
                // done
                break;
            }

            if (Cnt >= dwSkip)
                lpFrames[Cnt-dwSkip].dwReturnAddr = (DWORD)MapPtrProc(ControlPc,pCurProc);

            Cnt ++;

            // move to the next frame
            ControlPc = RtlVirtualUnwind(ControlPc,
                                      FunctionEntry,
                                      pCtx,
                                      &InFunction,
                                      &CurrFrame);

            // error if the frame is invalid
            if ((CurrFrame < LowLimit)
                || (CurrFrame > HighLimit)
                || ((CurrFrame & STK_ALIGN) != 0)) {
                err = ERROR_INVALID_PARAMETER;
                break;
            }
            
            // Set point at which control left the previous routine.
            if (((ControlPc == SYSCALL_RETURN)
                    || (ControlPc+INST_SIZE == SYSCALL_RETURN)
                    || (ControlPc == DIRECT_RETURN)
                    || (ControlPc+INST_SIZE == (DWORD) MD_CBRtn)
                    || (ControlPc == (DWORD) MD_CBRtn)
                    || (ControlPc+INST_SIZE == DIRECT_RETURN))) {

                if (!pcstk) {
                    err = ERROR_INVALID_PARAMETER;
                    break;
                }
                // across PSL boundary
                if (pcstk->dwPrevSP) {
					//
					// Cannot switch stack while getting callstack, or try-except will not work.
					// implication: non-trusted apps won't be able to get callstack post PSL boundary
					//
					break;
                    pCtx->IntSp = CurrFrame = pcstk->dwPrevSP + CALLEE_SAVED_REGS;
                    MDRestoreCalleeSavedRegisters (pcstk, pCtx);

                    // switch stack (by switch tlsPtr)
                    pth->tlsPtr = (pth->tlsPtr == pth->tlsSecure)? pth->tlsNonSecure : pth->tlsSecure;
                    NKGetStackLimits(pth, &LowLimit, &HighLimit);
                    PrevFrame = LowLimit;
                } else {
                    PrevFrame = CurrFrame;
                }

                // Update the return address and process context
                // information from the thread's call stack list.
                pCtx->IntRa = (ULONG)pcstk->retAddr;
                UpdateASID(pth, pcstk->pprcLast, pcstk->akyLast ? pcstk->akyLast : pth->aky);
                ControlPc = (ULONG) pCtx->IntRa - INST_SIZE;

                pcstk = pcstk->pcstkNext;
            } else { 
                if (pcstk && !pcstk->retAddr
                    && (PrevFrame > (DWORD) pcstk) && (CurrFrame <= (DWORD) pcstk)) {
                    UpdateASID(pth, pcstk->pprcLast, pcstk->akyLast ? pcstk->akyLast : pth->aky);
                    pcstk = pcstk->pcstkNext;
                }
                PrevFrame = CurrFrame;
            }
        } while (((ULONG) pCtx->IntSp < HighLimit) && ((ULONG) pCtx->IntSp > LowLimit));
#endif
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        err = ERROR_INVALID_PARAMETER;
    }
    // restore TLS settings
    pCurThread->tlsSecure[TLSSLOT_KERNEL] = saveKrnSecure;
    pCurThread->tlsNonSecure[TLSSLOT_KERNEL] = saveKrnNonSecure;

    KPlpvTls = pth->tlsPtr = tlsSave;
    UpdateASID(pth, pprcSave, akySave);

    if (Cnt < dwSkip) {
        err = ERROR_INVALID_PARAMETER;
    }
    if (err) {
        KSetLastError (pth, err);
        return 0;
    }

    return Cnt - dwSkip;
}


ULONG SC_GetCallStackSnapshot (ULONG dwMaxFrames, CallSnapshot lpFrames[], DWORD dwFlags, DWORD dwSkip)
{
    CONTEXT ctx;

    // validate arguments -- only support STACKSNAP_FAIL_IF_INCOMPLETE or no flags
    if ((dwFlags & ~STACKSNAP_FAIL_IF_INCOMPLETE)       // invalid flag
        || (dwSkip > MAX_SKIP)                          // too many skip
        || ((int) dwMaxFrames <= 0)                     // invalid maxframe
        || IsKernelVa (&ctx)                            // on KStack
        || !SC_MapPtrWithSize (lpFrames, dwMaxFrames * sizeof(CallSnapshot), hCurProc)) {  // invalid pointer
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return 0;
    }
#ifdef x86
    // for x86, we only need EBP to get the callstack
    __asm {
        mov  ctx.Ebp, ebp
    }
#else
    // get the current context
    RtlCaptureContext(&ctx);
#endif
    return NKGetCallStackSnapshot (dwMaxFrames, lpFrames, dwFlags, dwSkip + SKIP_FRAMES, &ctx);
}

