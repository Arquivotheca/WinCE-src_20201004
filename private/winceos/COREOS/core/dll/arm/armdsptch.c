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

#ifdef KCOREDLL
#define WIN32_CALL(type, api, args) (*(type (*) args) g_ppfnWin32Calls[W32_ ## api])
#else
#define WIN32_CALL(type, api, args) IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)
#endif


#include "kernel.h"
#include "excpt.h"
#include "coredll.h"
#include "errorrep.h"
#include "corexcpt.h"

extern void RtlDispatchReturn (void);

typedef struct _NONVOLATILE_CONTEXT {  
  
    ULONG R4;  
    ULONG R5;  
    ULONG R6;  
    ULONG R7;  
    ULONG R8;  
    ULONG R9;  
    ULONG R10;  
    ULONG R11;  
  
    ULONGLONG D8;  
    ULONGLONG D9;  
    ULONGLONG D10;  
    ULONGLONG D11;  
    ULONGLONG D12;  
    ULONGLONG D13;  
    ULONGLONG D14;  
    ULONGLONG D15;  
  
} NONVOLATILE_CONTEXT, *PNONVOLATILE_CONTEXT;  

#define NONVOL_INT_SIZE (8 * sizeof(ULONG))          // R4-R11
#define NONVOL_FP_SIZE  (8 * sizeof(ULONGLONG))      // D8-D15

BOOLEAN RtlUnwind(
    IN ULONG TargetFrame,
    IN ULONG TargetIp,
    IN PEXCEPTION_RECORD pExr,
    IN ULONG ReturnValue,
    IN PCONTEXT pCtx);


//
// Define private function prototypes.
//
PEXCEPTION_ROUTINE  
RtlVirtualUnwind (  
    __in ULONG ImageBase,  
    __in ULONG ControlPc,  
    __in PRUNTIME_FUNCTION FunctionEntry,  
    __inout PCONTEXT ContextRecord,  
    __out PVOID *HandlerData,  
    __out PULONG EstablisherFrame,  
    __inout_opt PVOID ContextPointers  
    );

ULONG  
RtlpGetFunctionEndAddress (  
    __in PRUNTIME_FUNCTION FunctionEntry,  
    __in ULONG ImageBase  
    )  
{  
    ULONG FunctionLength;  
      
    FunctionLength = FunctionEntry->UnwindData;  
    if ((FunctionLength & 3) != 0) {  
        FunctionLength = (FunctionLength >> 2) & 0x7ff;  
    } else {  
        FunctionLength = *(PULONG)(ImageBase + FunctionLength) & 0x3ffff;  
    }  
      
    return FunctionEntry->BeginAddress + 2 * FunctionLength;  
}  

  
PRUNTIME_FUNCTION  
RtlpSearchFunctionTable (  
    __in_ecount(EntryCount) PRUNTIME_FUNCTION FunctionTable,  
    __in ULONG EntryCount,  
    __in ULONG ControlPc,  
    __in ULONG ImageBase  
    )  
    /*++
        Routine Description:
            This function searches a sorted function table for a function entry
            containing the given program counter.
        Arguments:
            FunctionTable - Supplies a pointer to a sorted function table.
            EntryCount - Supplies the total number of entries in the function table.
            ControlPc - Supplies the actual program counter value.
            ImageBase - Supplies the address of the image base.
        Return Value:
            A pointer to the function entry containing the given program counter or
            NULL if no such entry exists.
    --*/    
{  
  
    PRUNTIME_FUNCTION FunctionEntry;  
    LONG High;  
    LONG Low;  
    LONG Middle;  
    ULONG RelativePc;  
  
    ASSERT(ControlPc >= ImageBase);  
  
    if (EntryCount == 0) {  
        return NULL;  
    }  
  
    //
    // Pre-check the last entry so that the loop can always rely on being able
    // to look at [entry + 1] for the start of the following function.
    //    
    RelativePc = ControlPc - ImageBase;  
    FunctionEntry = &FunctionTable[EntryCount - 1];  
    if (RelativePc < FunctionEntry->BeginAddress) {  
        Low = 0;  
        High = EntryCount - 2;  
        while (High >= Low) {  
  
            //
            // Compute next probe index and test entry. If the specified
            // control PC is greater than of equal to the beginning address
            // and less than the beginning address of the following function
            // table entry, then we found the possible candidate. Otherwise,
            // continue the search.
            //    
            Middle = (Low + High) >> 1;  
            FunctionEntry = &FunctionTable[Middle];  
            if (RelativePc < FunctionEntry->BeginAddress) {  
                High = Middle - 1;  
  
             } else if (RelativePc >= FunctionEntry[1].BeginAddress) {  
                Low = Middle + 1;  
  
            } else {  
                break;  
            }  
        }  
    }  
  
    //
    // Now verify the address against the true end of the function.
    //    
    if (RelativePc >= FunctionEntry->BeginAddress &&  
        RelativePc < RtlpGetFunctionEndAddress(FunctionEntry, ImageBase)) {  
  
        return FunctionEntry;  
    }  
  
    return NULL;  
} 


__inline BOOL IsAddrInModule (e32_lite *eptr, DWORD dwAddr)
{
    return (DWORD) (dwAddr - eptr->e32_vbase) < eptr->e32_vsize;
}


PRUNTIME_FUNCTION  
RtlLookupFunctionTable ( 
    e32_lite *eptr,
    ULONG ControlPc,
    ULONG *pCntEntries)
{
    // Lookup the function table entry using the point at which control left the procedure.
    PRUNTIME_FUNCTION FunctionTable = NULL;
    if (IsAddrInModule (eptr, ControlPc)
        || ExdReadPEHeader (hActiveProc, 0, ControlPc, eptr, sizeof (e32_lite))) {
        FunctionTable = (PRUNTIME_FUNCTION) (eptr->e32_vbase + eptr->e32_unit[EXC].rva);
        *pCntEntries  = eptr->e32_unit[EXC].size / sizeof (RUNTIME_FUNCTION);
    }

    return FunctionTable;
}

PRUNTIME_FUNCTION  
RtlLookupFunctionEntry ( 
    e32_lite *eptr,
    ULONG ControlPc)
{
    ULONG CntEntries = 0;
    PRUNTIME_FUNCTION FunctionTable = RtlLookupFunctionTable (eptr, ControlPc, &CntEntries);

    return FunctionTable
         ? RtlpSearchFunctionTable (FunctionTable, CntEntries, ControlPc, eptr->e32_vbase)
         : NULL;
}

//------------------------------------------------------------------------------
//
// helper function to perform exception handling, return ExceptionFlags, if ever
//
//------------------------------------------------------------------------------
BOOL RtlDispatchHelper (
    IN PEXCEPTION_RECORD      pExr,
    IN OUT DISPATCHER_CONTEXT *pdc,
    IN OUT DWORD              *pNestedFrame,
    IN OUT DWORD              *pExceptionFlags,
    IN OUT PCONTEXT           pCtx
)
{
    BOOL                    fRet             = FALSE;
    EXCEPTION_DISPOSITION   Disposition;
    PCONTEXT                pVirtualCtx      = pdc->ContextRecord;
    DWORD                   EstablisherFrame = pdc->EstablisherFrame;
    NONVOLATILE_CONTEXT     *pnvCtx          = (NONVOLATILE_CONTEXT *) pdc->NonVolatileRegisters;

    // The frame has an exception handler. The handler must be
    // executed by calling another routine that is written in
    // assembler. This is required because up level addressing
    // of the handler information is required when a nested
    // exception is encountered.

    do {

        pExr->ExceptionFlags = *pExceptionFlags;
        DEBUGMSG(DBGEH, (TEXT("Calling exception handler @%8.8lx(%8.8lx) Frame=%8.8lx\r\n"),
                pdc->LanguageHandler, pdc->HandlerData,
                pdc->EstablisherFrame));
        Disposition = RtlpExecuteHandlerForException (
                                        pExr,
                                        (LPVOID) EstablisherFrame,
                                        pCtx,
                                        pdc);

        *pExceptionFlags |= (pExr->ExceptionFlags & EXCEPTION_NONCONTINUABLE);

        DEBUGMSG(DBGEH, (TEXT("exception dispostion = %d\r\n"), Disposition));

        // If the current scan is within a nested context and the frame
        // just examined is the end of the nested region, then clear
        // the nested context frame and the nested exception flag in
        // the exception flags.
        if (*pNestedFrame == EstablisherFrame) {
            *pExceptionFlags &= (~EXCEPTION_NESTED_CALL);
            *pNestedFrame = 0;
        }

        // Case on the handler disposition.
        switch (Disposition) {

        // The disposition is to execute a frame based handler. The
        // target handler address is in DispatcherContext.ControlPc.
        // Reset to the original context and switch to unwind mode.
        case ExceptionExecuteHandler:
            DEBUGMSG(DBGEH, (TEXT("Unwinding to %8.8lx Frame=%8.8lx\r\n"),
                    pdc->ControlPc, EstablisherFrame));

            if (RtlUnwind (EstablisherFrame, 
                           pdc->ControlPc,
                           pExr, 
                           pExr->ExceptionCode,
                           pCtx)) {
                // continue execution from where Context record specifies
                DEBUGMSG(DBGEH, (TEXT("Continuing to Exception Handler at %8.8lx...\r\n"), CONTEXT_TO_PROGRAM_COUNTER (pCtx)));
                RtlResumeFromContext (pCtx);
                // never return
                ASSERT (0);
            }
            break;

        // The disposition is to continue execution.
        // If the exception is not continuable, then raise the
        // exception STATUS_NONCONTINUABLE_EXCEPTION. Otherwise
        // return exception handled.
        case ExceptionContinueExecution:

            if (!(pExr->ExceptionFlags & EXCEPTION_NONCONTINUABLE)) {
                // continue execution from where Context record specifies
                DEBUGMSG(DBGEH, (TEXT("Continuing execution at %8.8lx...\r\n"), CONTEXT_TO_PROGRAM_COUNTER (pCtx)));
                RtlResumeFromContext (pCtx);
                // never return
                ASSERT (0);
            }
            break;

        case ExceptionCollidedUnwind :

            *pVirtualCtx = *pdc->ContextRecord;
            memcpy (pnvCtx, pdc->NonVolatileRegisters, sizeof (*pnvCtx));

            RtlVirtualUnwind (pdc->ImageBase,
                              pdc->ControlPc,
                              pdc->FunctionEntry,
                              pVirtualCtx,
                              &pdc->HandlerData,
                              &EstablisherFrame,
                              NULL);

            EstablisherFrame          = pdc->EstablisherFrame;
            pdc->NonVolatileRegisters = (LPDWORD) pnvCtx;
            pdc->ContextRecord        = pVirtualCtx;
            
            break;


        // The disposition is nested exception.
        // Set the nested context frame to the establisher frame
        // address and set the nested exception flag in the
        // exception flags.
        case ExceptionNestedException :
            DEBUGMSG(DBGEH, (TEXT("Nested exception: frame=%8.8lx\r\n"),
                    pdc->EstablisherFrame));
            *pExceptionFlags |= EXCEPTION_NESTED_CALL;
            if (pdc->EstablisherFrame > *pNestedFrame)
                *pNestedFrame = pdc->EstablisherFrame;

            // fall-through to set return value to TRUE
            __fallthrough;

        // The disposition is to continue the search.
        // Get next frame address and continue the search.
        case ExceptionContinueSearch :
            fRet = TRUE;
            break;

        // All other disposition values are invalid.
        // Raise invalid disposition exception.
        default :
            RETAILMSG(1, (TEXT("RtlDispatchException: invalid dispositon (%d)\r\n"),
                    Disposition));
        }
    } while (ExceptionCollidedUnwind == Disposition);
    return fRet;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void DoRtlDispatchException (
    IN PEXCEPTION_RECORD pExr,
    IN PCONTEXT pCtx
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
    pExr - Supplies a pointer to an exception record.
    pCtx - Supplies a pointer to a context record.

Return Value:
    If the exception is handled by one of the frame based handlers, then
    a value of TRUE is returned. Otherwise a value of FALSE is returned.
--*/
{
    e32_lite e32            = {0};
    PCONTEXT pOrigCtx       = pCtx;          // orignal context, for unhandled exception reporting
    CONTEXT  DispatchCtx    = *pCtx;
    CONTEXT  VirtualCtx     = *pCtx;
    DWORD    ControlPc      = pCtx->Pc;
    DWORD    NestedFrame    = 0;
    DWORD    ExceptionFlags = pExr->ExceptionFlags & EXCEPTION_NONCONTINUABLE;   // preserve 'non-continuable' flag
    const UINT LowLimit     = UStkBound ();
    const UINT HighLimit    = GetStkHighLimit ();
    DISPATCHER_CONTEXT  dc;
    PRUNTIME_FUNCTION   FunctionEntry;
    NONVOLATILE_CONTEXT nvCtx;
    PEXCEPTION_ROUTINE  ExceptionRoutine;  
    PVOID               HandlerData;  
    DWORD               PrevSP;
    DWORD               EstablisherFrame;
    BOOL                fFirstIteration = TRUE;

    DEBUGMSG(DBGEH, (TEXT("RtlDispatchException(%08x): ControlPc=%8.8lx SP=%8.8lx, PSR = %8.8lx\r\n"),
            pCtx, ControlPc, pCtx->IntSp, pCtx->Psr));

    // use local context to dispatch exception such that we still have the original context
    // for exception reporting.
    pCtx = &VirtualCtx;

    // clear "in callback" flag.
    pExr->ExceptionFlags &= ~EXCEPTION_FLAG_IN_CALLBACK;

    // call vector exceptoin handlers
    // NOTE: CallVectorHandler does NOT return if Vector handlers take care of exception.
    CallVectorHandlers (pExr, pCtx);

    // look for function entry
    FunctionEntry = RtlLookupFunctionEntry (&e32, ControlPc);

    if (!FunctionEntry) {
        // excepiton in LEAF function, get to caller
        if (pCtx->Pc != pCtx->Lr) {
            pCtx->Pc  = pCtx->Lr;
            if (pCtx->Pc) {
                ControlPc = pCtx->Pc - INST_SIZE;
                FunctionEntry = RtlLookupFunctionEntry (&e32, ControlPc);
            }
        }
    }

    while (FunctionEntry) {

        BOOL fInRtlDispatch = ((DWORD) RtlDispatchReturn == pCtx->Pc);

        PrevSP = pCtx->Sp;

        if (!IsValidStkPtr (PrevSP, LowLimit, HighLimit)) {
            ExceptionFlags |= EXCEPTION_STACK_INVALID;
            break;
        }

        // save non-volatile context before VirtualUnwind
        memcpy (&nvCtx.R4, &pCtx->R4, NONVOL_INT_SIZE);
        memcpy (&nvCtx.D8, &pCtx->D[8], NONVOL_FP_SIZE);

        ExceptionRoutine = RtlVirtualUnwind (e32.e32_vbase,
                                             ControlPc,
                                             FunctionEntry,
                                             pCtx,
                                             &HandlerData,
                                             &EstablisherFrame,
                                             NULL);
        
        
        DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: RtlVirtualUnwind returns, ExceptionRoutine=%8.8lx EstablisherFrame=%8.8lx, NextPC = %8.8lx\r\n"),
                        ExceptionRoutine, EstablisherFrame, pCtx->Pc));

        if (!IsValidStkPtr (EstablisherFrame, LowLimit, HighLimit)) {
            ExceptionFlags |= EXCEPTION_STACK_INVALID;
            break;
        }

        if (ExceptionRoutine) {
            dc.ContextRecord    = pCtx;
            dc.ControlPc        = ControlPc;
            dc.EstablisherFrame = EstablisherFrame;
            dc.FunctionEntry    = FunctionEntry;
            dc.HandlerData      = HandlerData;
            dc.ImageBase        = e32.e32_vbase;
            dc.LanguageHandler  = ExceptionRoutine;
            dc.ScopeIndex       = 0;
            dc.NonVolatileRegisters = (DWORD *)&nvCtx;

            if (!RtlDispatchHelper (pExr,
                                    &dc,
                                    &NestedFrame,
                                    &ExceptionFlags,
                                    &DispatchCtx)) {
                // dispatch failed, report unhandled exception
                break;
            }
        }

        // sp has to be strict increasing
        if (pCtx->Sp < PrevSP) {
            break;
        }

        // if we fault at the 1st push instruction of a function, the new SP will
        // be the same as the previous SP after the first unwind. Need to continue the loop in this case.
        if ((pCtx->Sp == PrevSP) && !fFirstIteration) {
            break;
        }
        fFirstIteration = FALSE;

        // break if reaches PSL boundary
        if (IsPSLBoundary (pCtx->Pc)) {
            break;
        }

        if (fInRtlDispatch || !pCtx->Pc) {
            // special case when we unwind across RtlDispatchException, where pCtx->Pc is
            // exactly the point of exception
            ControlPc = pCtx->Pc;
        } else {

            ControlPc = pCtx->Pc - INST_SIZE;
        }

        FunctionEntry = RtlLookupFunctionEntry (&e32, ControlPc);

    }
    
    if (IsPSLBoundary (pCtx->Pc)) {
        DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: Handler not found, perform Exit-Unwind pc=%x code=%x\n"),
            CONTEXT_TO_PROGRAM_COUNTER (pCtx), pExr->ExceptionCode));

        // perform an EXIT_UNWIND if cross PSL boundary
        RtlUnwind (0, 0, pExr, pExr->ExceptionCode, &DispatchCtx);

        // try global handler
        *pCtx = *pOrigCtx;
        CallGlobalHandler (pCtx, pExr);

    }

    pExr->ExceptionFlags = ExceptionFlags;

    // Set final exception flags and return exception not handled.
    DEBUGMSG(1, (TEXT("RtlDispatchException: returning failure. Flags=%x\r\n"), ExceptionFlags));

    RtlReportUnhandledException (pExr, pOrigCtx);
    // should never return
    DEBUGCHK (0);
    
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOLEAN
RtlUnwind (
    IN ULONG TargetFrame,
    IN ULONG TargetPc,
    IN PEXCEPTION_RECORD pExr,
    IN ULONG ReturnValue,
    IN PCONTEXT pOrigCtx
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

    TargetFrame - Supplies an optional pointer to the call frame that is the
        target of the unwind. If this parameter is not specified, then an exit
        unwind is performed.

    TargetIp - Supplies an optional instruction address that specifies the
        continuation address of the unwind. This address is ignored if the
        target frame parameter is not specified.

    pExr - Supplies an optional pointer to an exception record.

    ReturnValue - Supplies a value that is to be placed in the integer
        function return register just before continuing execution.

    pCtx - Supplies a pointer to a context record that can be used
        to store context druing the unwind operation.

Return Value:
    None.
--*/
{
    e32_lite e32            = {0};
    CONTEXT  LocalCtx;
    PCONTEXT pCtx           = pOrigCtx;         // current context
    PCONTEXT pPrevCtx       = &LocalCtx;        // previous context
    PCONTEXT pTmpCtx;                           // tmp context pointer for swapping
    DWORD    ControlPc      = pCtx->Pc;
    DWORD    ExceptionFlags = pExr->ExceptionFlags & EXCEPTION_NONCONTINUABLE;   // preserve 'non-continuable' flag
    DWORD  EstablisherFrame = pCtx->Sp;
    BOOLEAN  fRet           = FALSE;
    const UINT LowLimit     = UStkBound ();
    const UINT HighLimit    = GetStkHighLimit ();
    DISPATCHER_CONTEXT  dc;
    PRUNTIME_FUNCTION   FunctionEntry;
    NONVOLATILE_CONTEXT nvCtx;
    PEXCEPTION_ROUTINE  ExceptionRoutine;  
    PVOID               HandlerData;  

    DEBUGMSG(DBGEH, (TEXT("RtlUnwind(%08x): ControlPc=%8.8lx SP=%8.8lx, PSR = %8.8lx\r\n"),
            pCtx, ControlPc, pCtx->IntSp, pCtx->Psr));

    ExceptionFlags = EXCEPTION_UNWINDING;

    if (!TargetFrame) {
        ExceptionFlags |= EXCEPTION_EXIT_UNWIND;
    }

    // look for function entry
    FunctionEntry = RtlLookupFunctionEntry (&e32, ControlPc);

    if (!FunctionEntry) {
        // excepiton in LEAF function, get to caller
        if (pCtx->Pc != pCtx->Lr) {
            pCtx->Pc  = pCtx->Lr;
            if (pCtx->Pc) {
                ControlPc = pCtx->Pc - INST_SIZE;
                FunctionEntry = RtlLookupFunctionEntry (&e32, ControlPc);
            }
        }
    }

    while (FunctionEntry) {
        
        BOOL fInRtlDispatch = ((DWORD) RtlDispatchReturn == pCtx->Pc);

        memcpy (&nvCtx.R4, &pCtx->R4, NONVOL_INT_SIZE);
        memcpy (&nvCtx.D8, &pCtx->D[8], NONVOL_FP_SIZE);

        *pPrevCtx = *pCtx;
        
        ExceptionRoutine = RtlVirtualUnwind (e32.e32_vbase,
                                             ControlPc,
                                             FunctionEntry,
                                             pPrevCtx,
                                             &HandlerData,
                                             &EstablisherFrame,
                                             NULL);


        DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: RtlVirtualUnwind returns, ExceptionRoutine=%8.8lx EstablisherFrame=%8.8lx, NextPC = %8.8lx\r\n"),
                        ExceptionRoutine, EstablisherFrame, pCtx->Pc));

        // If the virtual frame pointer is not within the specified stack
        // limits, the virtual frame pointer is unaligned, or the target
        // frame is below the virtual frame and an exit unwind is not being
        // performed, then raise the exception STATUS_BAD_STACK. Otherwise,
        // check to determine if the current routine has an exception
        // handler.
        if (!IsValidStkPtr (EstablisherFrame, LowLimit, HighLimit)) {
            break;
        }

        if ((pPrevCtx->Sp == pCtx->Sp) && (pPrevCtx->Pc == pCtx->Pc)) {
            // no progress is made, bad function table
            break;
        }


        if (ExceptionRoutine) {

            EXCEPTION_DISPOSITION   Disposition;

            pExr->ExceptionFlags    = ExceptionFlags;
            dc.TargetPc             = TargetPc;
            dc.ContextRecord        = pCtx;
            dc.ControlPc            = ControlPc;
            dc.EstablisherFrame     = EstablisherFrame;
            dc.FunctionEntry        = FunctionEntry;
            dc.HandlerData          = HandlerData;
            dc.ImageBase            = e32.e32_vbase;
            dc.LanguageHandler      = ExceptionRoutine;
            dc.ScopeIndex           = 0;
            dc.NonVolatileRegisters = (DWORD *)&nvCtx;

            do {

                DEBUGMSG(DBGEH, (TEXT("Calling unwind handler @%8.8lx(%8.8lx) Frame=%8.8lx\r\n"),
                        ExceptionRoutine, HandlerData, EstablisherFrame));

                // If the establisher frame is the target of the unwind
                // operation, then set the target unwind flag.
                if (TargetFrame == EstablisherFrame) {
                    ExceptionFlags |= EXCEPTION_TARGET_UNWIND;
                }
                
                pExr->ExceptionFlags = ExceptionFlags;

                //
                // Set the specified return value and target IP in case
                // the exception handler directly continues execution.
                //
                pCtx->R0 = ReturnValue;
                Disposition = RtlpExecuteHandlerForUnwind (pExr,
                                                           (LPVOID) EstablisherFrame,
                                                           pCtx,
                                                           &dc);

                DEBUGMSG(DBGEH, (TEXT("exception dispostion = %d\r\n"), Disposition));

                ExceptionFlags &=
                    ~(EXCEPTION_COLLIDED_UNWIND | EXCEPTION_TARGET_UNWIND);

                switch (Disposition) {

                    //
                    // The disposition is to continue the search.
                    //
                    // If the target frame has not been reached, then
                    // swap context pointers.
                    //

                case ExceptionContinueSearch :
                    if (EstablisherFrame != TargetFrame) {
                        pTmpCtx  = pCtx;
                        pCtx     = pPrevCtx;
                        pPrevCtx = pTmpCtx;
                    }

                    break;

                    //
                    // The disposition is collided unwind.
                    //
                    // Copy the context of the previous unwind and
                    // virtually unwind to the caller of the extablisher,
                    // then set the target of the current unwind to the
                    // dispatcher context of the previous unwind, and
                    // reexecute the exception handler from the collided
                    // frame with the collided unwind flag set in the
                    // exception record.
                    //

                case ExceptionCollidedUnwind :
                    *pOrigCtx       = *dc.ContextRecord;
                    memcpy (&nvCtx, dc.NonVolatileRegisters, sizeof (nvCtx));

                    pCtx            = pOrigCtx;
                    pPrevCtx        = &LocalCtx;
                    *pPrevCtx       = *pCtx;

                    RtlVirtualUnwind (dc.ImageBase,
                                      dc.ControlPc,
                                      dc.FunctionEntry,
                                      pPrevCtx,
                                      &dc.HandlerData,
                                      &EstablisherFrame,
                                      NULL);

                    EstablisherFrame = dc.EstablisherFrame;
                    dc.ContextRecord = pCtx;
                    dc.NonVolatileRegisters = (LPDWORD) &nvCtx;
                    
                    ExceptionFlags  |= EXCEPTION_COLLIDED_UNWIND;
                    
                    break;

                    //
                    // All other disposition values are invalid.
                    //
                    // Raise invalid disposition exception.
                    //

                default :
                    return FALSE;
                };
                
            } while (ExceptionFlags & EXCEPTION_COLLIDED_UNWIND);
            
        } else {
            //
            // If the target frame has not been reached, then
            // swap context pointers.
            //

            if (EstablisherFrame != TargetFrame) {
                pTmpCtx  = pCtx;
                pCtx     = pPrevCtx;
                pPrevCtx = pTmpCtx;
            }
        }

        // target frame reached?
        if (EstablisherFrame == TargetFrame) {
            if (pCtx != pOrigCtx) {
                *pOrigCtx = *pCtx;
                pCtx = pOrigCtx;
            }

            pCtx->R0 = ReturnValue;
            pCtx->Pc = dc.TargetPc;

            DEBUGMSG(DBGEH, (TEXT("RtlUnwind: continuing @%8.8lx\r\n"), CONTEXT_TO_PROGRAM_COUNTER(pCtx)));
            fRet = TRUE;
            break;
        }

        pCtx->Pc = pCtx->Lr;

        if (fInRtlDispatch || !pCtx->Pc) {
            // special case when we unwind across RtlDispatchException, where pCtx->Pc is
            // exactly the point of exception
            ControlPc = pCtx->Pc;
        } else {

            ControlPc = pCtx->Pc - INST_SIZE;
        }

        if (IsPSLBoundary (pCtx->Pc)) {

            DEBUGMSG (DBGEH, (L"PSL Boundary, ControlPc = %8.8lx, TargetFrame = %8.8lx, TargetIp = %8.8lx\r\n", ControlPc, TargetFrame, dc.TargetPc));
            break;
        }

        FunctionEntry = RtlLookupFunctionEntry (&e32, ControlPc);

    }

    return fRet;
}


#ifdef KCOREDLL

FARPROC *g_ppfnNetCfCalls;          // Net CF APIs
#define NETCF_UNWIND_FRAME 27       // API index
#define RtlUnwindOneManagedFrame    (*(DWORD (*) (DWORD, PCONTEXT, LPDWORD, LPDWORD, DWORD)) g_ppfnNetCfCalls[NETCF_UNWIND_FRAME])

DWORD RtlUnwindOneFrame (
    IN DWORD ControlPc,
    IN LPDWORD pTlsData,
    IN OUT PCONTEXT pCtx,
    IN OUT e32_lite *eptr,
    IN OUT LPDWORD pMgdFrameCookie
    )
{
    DWORD RtnAddr = 0;
    
    // first give managed unwinder a chance to look at the frame
    if (pMgdFrameCookie) {
        if (!g_ppfnNetCfCalls) {
            const CINFO **_SystemAPISets = (const CINFO **)(UserKInfo[KINX_APISETS]);
            const CINFO * pci = (const CINFO *)_SystemAPISets[SH_NETCF];
            g_ppfnNetCfCalls = (FARPROC*)(pci? pci->ppfnIntMethods : NULL);            
        }
        if (g_ppfnNetCfCalls) {
            RtnAddr = RtlUnwindOneManagedFrame(ControlPc, pCtx, pTlsData, pMgdFrameCookie, 0/* reserved */);
        }

    }

    if (!RtnAddr) {

        PRUNTIME_FUNCTION prf = RtlLookupFunctionEntry (eptr, ControlPc);

        if (prf) {
            DWORD dummy;
            RtlVirtualUnwind(eptr->e32_vbase,
                             ControlPc,
                             prf,
                             pCtx,
                             (PVOID *)&dummy,
                             &dummy,
                             NULL);
            RtnAddr = pCtx->Pc;
        }
    }

    return RtnAddr;
}

#endif // KCOREDLL


