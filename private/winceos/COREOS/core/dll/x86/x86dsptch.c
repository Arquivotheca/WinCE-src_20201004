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

extern BOOL g_fDepEnabled;

BOOLEAN
RtlUnwind(
    IN PEXCEPTION_REGISTRATION_RECORD TargetFrame,
    IN ULONG TargetIp,
    IN PEXCEPTION_RECORD pExr,
    IN ULONG ReturnValue,
    IN PCONTEXT pCtx
    );

#ifdef DEBUG
int DumpContext (PCONTEXT pCtx)
{
    NKDbgPrintfW(L"Eax=%8.8lx Ebx=%8.8lx Ecx=%8.8lx Edx=%8.8lx\r\n",
            pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx);
    NKDbgPrintfW(L"Esi=%8.8lx Edi=%8.8lx Ebp=%8.8lx Esp=%8.8lx Eip=%8.8lx\r\n",
            pCtx->Esi, pCtx->Edi, pCtx->Ebp, pCtx->Esp, pCtx->Eip);
    NKDbgPrintfW(L"CS=%4.4lx DS=%4.4lx ES=%4.4lx SS=%4.4lx FS=%4.4lx GS=%4.4lx\r\n",
            pCtx->SegCs, pCtx->SegDs, pCtx->SegEs, pCtx->SegSs, pCtx->SegFs, pCtx->SegGs);

    return 0;
}
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD RtlUnwindOneFrame (
    IN DWORD ControlPc,
    IN LPDWORD pTlsData,
    IN OUT PCONTEXT pCtx,
    IN OUT e32_lite *eptr,
    IN OUT LPDWORD pMgdFrameCookie
    )
{
    // x86 does not support frame unwinding
    DEBUGCHK (0);
    return 0;
}

//------------------------------------------------------------------------------
// Report an invalid handler to Windows Error Reporting (WER) so that there is
// some indication why the application was terminated.
//------------------------------------------------------------------------------
static
VOID
RtlInvalidHandlerDetected(
    IN PVOID Handler,
    IN PEXCEPTION_RECORD pExr,
    IN PCONTEXT pCtx
    )
{
    EXCEPTION_RECORD   AvExceptionRecord = {0};
    EXCEPTION_POINTERS ExceptionPointers;

    // Build an exception record
    // Note: We use the /GS failure exception code here to make these
    //       stand out in Watson dumps.  An accurate representation of
    //       of the situation would be
    //
    //           ExceptionCode = STATUS_ACCESS_VIOLATION
    //           NumberParameters = 1;
    //           ExceptionInformation[0] = EXCEPTION_EXECUTE_FAULT (8)
    //
    //       This is what would show up if H/W DEP were available.

    AvExceptionRecord.ExceptionCode = (DWORD)STATUS_STACK_BUFFER_OVERRUN;
    AvExceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    AvExceptionRecord.ExceptionRecord = pExr; // chain the record
    AvExceptionRecord.ExceptionAddress = Handler;

    // Combine the exception and context records
    ExceptionPointers.ExceptionRecord = &AvExceptionRecord;
    ExceptionPointers.ContextRecord   = pCtx;

    // Invoke Watson (WER)
    ReportFault(&ExceptionPointers, 0);
}

//------------------------------------------------------------------------------
// Find the image's structured exception handler table via the Load
// Configuration structure, if present.
//------------------------------------------------------------------------------
static
PULONG
RtlLookupFunctionTable (
    IN  PEXCEPTION_ROUTINE ExceptionRoutine,
    OUT ULONG_PTR* ImageBase,
    OUT PULONG FunctionTableLength
    )
{
    e32_lite e32;
    PIMAGE_LOAD_CONFIG_DIRECTORY32 LoadConfig;
    DWORD MinSize = RTL_SIZEOF_THROUGH_FIELD(IMAGE_LOAD_CONFIG_DIRECTORY, SEHandlerCount);
    ULONG Rva;
    ULONG Size;

    *ImageBase = 0;

    if (ExdReadPEHeader(hActiveProc, 0, (DWORD)ExceptionRoutine, &e32, sizeof(e32)))
    {
        *ImageBase = e32.e32_vbase;

        Rva = e32.e32_unit[EXC].rva;

        if (Rva & E32_EXC_NO_SEH) {
            // Handlers are not allowed in this image
            *FunctionTableLength = (ULONG)-1;
            return (PULONG)-1;
        }

        // Get rid of the extra bits stored in the RVA
        Rva &= ~(E32_EXC_NO_SEH | E32_EXC_DEP_COMPAT);

        Size = e32.e32_unit[EXC].size;

        if ((Rva != 0) &&
            (Size >= RTL_SIZEOF_THROUGH_FIELD(IMAGE_LOAD_CONFIG_DIRECTORY, Size))) {

            LoadConfig = (PIMAGE_LOAD_CONFIG_DIRECTORY32)(*ImageBase + Rva);

            // Note: Due to a backward compat issue on the desktop, the linker
            //       always encodes a size of 0x40 in the Load Config data
            //       directory entry, so checking that value is useless.
            //       Instead, just verify that the Size field in the structure
            //       itself indicates the SEHandler entries are present.

            if (LoadConfig->Size >= MinSize) {
                // SE Handler table found
                *FunctionTableLength = LoadConfig->SEHandlerCount;
                return (PULONG)LoadConfig->SEHandlerTable;
            }
        }

        // Image is not SafeSEH aware
    }

    *FunctionTableLength = 0;
    return NULL;
}

//------------------------------------------------------------------------------
// Determine whether it is safe to dispatch to an exception handler.
//------------------------------------------------------------------------------
static
BOOLEAN
RtlIsValidHandler (
    IN PEXCEPTION_ROUTINE Handler
    )
{
    PULONG FunctionTable;
    ULONG FunctionTableLength;
    ULONG_PTR Base = 0;
    ULONG_PTR HandlerAddress;
#ifndef KCOREDLL
    MEMORY_BASIC_INFORMATION mbi;
#endif

    FunctionTable = RtlLookupFunctionTable(Handler, &Base, &FunctionTableLength);

    if (FunctionTable && FunctionTableLength) {
        ULONG_PTR FunctionEntry;
        LONG High, Middle, Low;

        if ((FunctionTable == (PULONG)-1) && (FunctionTableLength == (ULONG)-1)) {
            // Address is in an image that shouldn't have any handlers (like a resource-only dll).
            DEBUGMSG(DBGEH, (TEXT("RtlIsValidHandler: Handler %8.8lx in image without handlers\r\n"), Handler));
            return FALSE;
        }

        // Bias the handler value down by the image base and see if the result
        // is in the table

        HandlerAddress = (ULONG_PTR)Handler - Base;
        Low = 0;
        High = FunctionTableLength;
        while (High >= Low) {
            Middle = (Low + High) >> 1;
            FunctionEntry = (ULONG_PTR)FunctionTable[Middle];
            if (HandlerAddress < FunctionEntry) {
                High = Middle - 1;
            } else if (HandlerAddress > FunctionEntry) {
                Low = Middle + 1;
            } else {
                // found it
                DEBUGMSG(DBGEH, (TEXT("RtlIsValidHandler: Handler %8.8lx found in SE handler table\r\n"), Handler));
                return TRUE;
            }
        }

        // Didn't find it
        DEBUGMSG(DBGEH, (TEXT("RtlIsValidHandler: Handler %8.8lx not in SE handler table\r\n"), Handler));
        return FALSE;
    }

#ifndef KCOREDLL
    // We were unable to find the image function table - let's see if it looks reasonable
    if (VirtualQueryEx(GetCurrentProcess(), (PVOID)(LONG)Handler, &mbi, sizeof(mbi)) != 0) {
        if (mbi.Protect & (PAGE_EXECUTE |
                           PAGE_EXECUTE_READ |
                           PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY)) {
            // Executable memory--must be JIT'd code or non-SafeSEH image.
            // Good enough for now.
            DEBUGMSG(DBGEH, (TEXT("RtlIsValidHandler: Handler %8.8lx in executable memory\r\n"), Handler));
            return TRUE;
        } else {
            // Not executable.  The secure thing to do is to indicate an
            // invalid handler, but we can only do this if all loaded images
            // have indicated they are compatible with Data Execution
            // Prevention (DEP).

            if (g_fDepEnabled) {
                DEBUGMSG(DBGEH, (TEXT("RtlIsValidHandler: Handler %8.8lx not in executable memory\r\n"), Handler));
                return FALSE;
            } else {
                RETAILMSG(1, (TEXT("RtlIsValidHandler: WARNING: Handler %8.8lx not in executable memory\r\n"), Handler));
                return TRUE;
            }
        }
    }

    // Unverifiable--give the handler the benefit of the doubt, but
    // VirtualQuery should not fail if the handler is valid, so notify
    // with a DEBUGCHK.  We would almost certainly crash when the dispatch
    // happens anyway.
    DEBUGMSG(DBGEH, (TEXT("RtlIsValidHandler: Handler %8.8lx cannot be verified\r\n"), Handler));
    DEBUGCHK(0);

    return TRUE;
    
#else
    // We don't support JIT'ing exception handlers in kernel mode, so only
    // allow handlers in images.
    if (Base == 0) {
        DEBUGMSG(DBGEH, (TEXT("RtlIsValidHandler: Kernel-mode handler %8.8lx not in an image\r\n"), Handler));
        return FALSE;
    } else {
        DEBUGMSG(DBGEH, (TEXT("RtlIsValidHandler: Kernel-mode handler %8.8lx in an image (%8.8lx)\r\n"), Handler, Base));
        return TRUE;
    }
#endif
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void RtlDispatchException (
    IN OUT PEXCEPTION_RECORD pExr,
    IN PCONTEXT pCtx
    )
{
    EXCEPTION_DISPOSITION Disposition;
    PEXCEPTION_REGISTRATION_RECORD pRP;
    PEXCEPTION_REGISTRATION_RECORD pNestedRP;
    DISPATCHER_CONTEXT DispatcherContext;
    CONTEXT ctx = *pCtx;        // make a local copy of the context
    PCONTEXT pOrigCtx = pCtx;

    const UINT LowLimit = UStkBound ();
    const UINT HighLimit = GetStkHighLimit ();

    DEBUGMSG(DBGEH, (TEXT("RtlDispatchException (%x %x) pc %x code %x address %x, flag %x\r\n"),
        pExr, pCtx, pCtx->Eip,
        pExr->ExceptionCode, pExr->ExceptionAddress, pExr->ExceptionFlags));

    // call vector exceptoin handlers
    // NOTE: CallVectorHandler does NOT return if Vector handlers take care of exception.
    CallVectorHandlers (pExr, pCtx);

    pCtx = &ctx;        // operate on local context

    /*
       Start with the frame specified by the context record and search
       backwards through the call frame hierarchy attempting to find an
       exception handler that will handler the exception.
     */
    pRP = GetRegistrationHead();
    pNestedRP = 0;

    DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: RP=%8.8lx, LowLimit %8.8lx, HighLimit %8.8lx\r\n"), pRP, LowLimit, HighLimit));

    while (IsValidStkPtr ((DWORD) pRP, LowLimit, HighLimit)) {

        // validation of exception handler address is kind of meaningless for x86:
        // - x86 exception directory is always empty. we can't validate the exact value.
        // - use of "VirtualQuery" to test if the address is executable is useless as it can 
        //   be pointing to any code, even the middle of an instruction. 
        // - use of "VirtualQuery" doesn't work for code pages that aren't paged in, and we
        //   can end up rejecting valid handler.
#if 0
        /* See if the handler is reasonable
         */

        if (!RtlIsValidHandler (pRP->Handler)) {
            DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: Dispatch blocked")));
            RtlInvalidHandlerDetected ((PVOID)(LONG)(pRP->Handler), pExr, pOrigCtx);
            pExr->ExceptionFlags |= EXCEPTION_STACK_INVALID;
            RtlReportUnhandledException (pExr, pOrigCtx);
        }
#endif
        /* The handler must be executed by calling another routine
         that is written in assembler. This is required because
         up level addressing of the handler information is required
         when a nested exception is encountered.
         */

        DispatcherContext.ControlPc = 0;
        DispatcherContext.RegistrationPointer = pRP;

        DEBUGMSG(DBGEH, (TEXT("Calling exception handler @%8.8lx RP=%8.8lx\r\n"),
                pRP->Handler, pRP));

        Disposition = RtlpExecuteHandlerForException(pExr,
                pRP, pCtx, &DispatcherContext,
                pRP->Handler);

        DEBUGMSG(DBGEH, (TEXT("exception dispostion = %d\r\n"), Disposition));

        /* If the current scan is within a nested context and the frame
           just examined is the end of the context region, then clear
           the nested context frame and the nested exception in the
           exception flags.
         */
        if (pNestedRP == pRP) {
            pExr->ExceptionFlags &= (~EXCEPTION_NESTED_CALL);
            pNestedRP = 0;
        }

        // Case on the handler disposition.
        switch (Disposition) {

        // The disposition is to execute a frame based handler. The
        // target handler address is in DispatcherContext.ControlPc.
        // Reset to the original context and switch to unwind mode.
        case ExceptionExecuteHandler:
            DEBUGMSG(DBGEH && DispatcherContext.ControlPc,
                    (TEXT("Unwinding to %8.8lx pRP=%8.8lx\r\n"),
                    DispatcherContext.ControlPc, pRP));
            DEBUGMSG(DBGEH && !DispatcherContext.ControlPc,
                    (TEXT("Unwinding to %8.8lx pRP=%8.8lx Ebx=%8.8lx Esi=%d\r\n"),
                    pCtx->Eip, pRP,
                    pCtx->Ebx, pCtx->Esi));

            if (RtlUnwind (pRP,
                    DispatcherContext.ControlPc, pExr,
                    pExr->ExceptionCode, pCtx)) {

                // continue execution from where Context record specifies
                DEBUGMSG(DBGEH, (TEXT("Continuing to Exception Handler at %8.8lx...\r\n"), pCtx->Eip));
                RtlResumeFromContext (pCtx);
            }

            pExr->ExceptionFlags |= EXCEPTION_NONCONTINUABLE;
            // fall through to report unhandled Exception
            __fallthrough;

        /* The disposition is to continue execution. If the
           exception is not continuable, then raise the exception
           EXC_NONCONTINUABLE_EXCEPTION. Otherwise return
           TRUE. */
        case ExceptionContinueExecution :
            if (pExr->ExceptionFlags & EXCEPTION_NONCONTINUABLE) {
                // try to continue on a non-continuable exception
                RtlReportUnhandledException (pExr, pOrigCtx);
            } else {
                // continue execution from where Context record specifies
                DEBUGMSG(DBGEH, (TEXT("Continuing execution at %8.8lx...\r\n"), pCtx->Eip));
                RtlResumeFromContext (pCtx);
            }
            // both of the above should not return at all
            DEBUGCHK (0);
            break;

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
            pExr->ExceptionFlags |= EXCEPTION_NESTED_CALL;

            if (DispatcherContext.RegistrationPointer > pNestedRP){
                pNestedRP = DispatcherContext.RegistrationPointer;
            }
            break;

            /*
               All other disposition values are invalid. Raise
               invalid disposition exception.
             */

        default :
            RETAILMSG(1, (TEXT("RtlDispatchException: invalid dispositon (%d)\r\n"),
                    Disposition));
            RtlReportUnhandledException (pExr, pOrigCtx);
            DEBUGCHK (0);
            break;
        }

        pRP = pRP->Next;

    }


    // cross PSL boundary?
    if ((PEXCEPTION_REGISTRATION_RECORD) REGISTRATION_RECORD_PSL_BOUNDARY == pRP) {
        DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: Handler not found, perform Exit-Unwind pc=%x code=%x\n"),
            pCtx->Eip, pExr->ExceptionCode));

        // perform an EXIT_UNWIND if cross PSL boundary
        RtlUnwind (NULL, 0, pExr, pExr->ExceptionCode, pCtx);

        // try global handler
        *pCtx = *pOrigCtx;
        CallGlobalHandler (pCtx, pExr);
    }


    // Set final exception flags and return exception not handled.
    DEBUGMSG(1, (TEXT("URtlDispatchException: returning failure. Flags=%x\r\n"), pExr->ExceptionFlags, DumpContext(pOrigCtx)));

    RtlReportUnhandledException (pExr, pOrigCtx);
    DEBUGCHK (0);
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOLEAN
RtlUnwind(
    IN PEXCEPTION_REGISTRATION_RECORD TargetFrame,
    IN ULONG TargetIp,
    IN PEXCEPTION_RECORD pExr,
    IN ULONG ReturnValue,
    IN PCONTEXT pCtx
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
    TRUE if target frame or callstack reached.
--*/
{
    DISPATCHER_CONTEXT DispatcherContext;
    EXCEPTION_DISPOSITION Disposition;
    PEXCEPTION_REGISTRATION_RECORD pRP;
    ULONG ExceptionFlags;
    const UINT LowLimit = UStkBound ();
    const UINT HighLimit = GetStkHighLimit ();
    BOOLEAN continueLoop = TRUE;

    // If the target frame of the unwind is specified, then set EXCEPTION_UNWINDING
    // flag in the exception flags. Otherwise set both EXCEPTION_EXIT_UNWIND and
    // EXCEPTION_UNWINDING flags in the exception flags.
    ExceptionFlags = pExr->ExceptionFlags | EXCEPTION_UNWINDING;
    if (!TargetFrame)
        ExceptionFlags |= EXCEPTION_EXIT_UNWIND;

    pCtx->Eax = ReturnValue;
    if (TargetIp)
        pCtx->Eip = TargetIp;

    // Scan backward through the call frame hierarchy, calling exception
    // handlers as they are encountered, until the target frame of the unwind
    // is reached.
    pRP = GetRegistrationHead ();

    DEBUGMSG(DBGEH, (TEXT("RtlUnwind: TargetFrame=%8.8lx, TargetIp = %8.8lx, RP = %8.8lx, ll = %8.8lx, hl = %8.8lx\r\n"),
        TargetFrame, TargetIp, pRP, LowLimit, HighLimit));

    while (IsValidStkPtr ((DWORD) pRP, LowLimit, HighLimit)) {

        DEBUGMSG(DBGEH, (TEXT("RtlUnwind: RP=%8.8lx\r\n"), pRP));

        do {
            if (pRP == TargetFrame){
                // Establisher frame is the target of the unwind
                // operation.  Set the target unwind flag.  We still
                // need to excute the loop body one more time, so we
                // can't just break out.
                ExceptionFlags |= EXCEPTION_TARGET_UNWIND;
                continueLoop = FALSE;
            }
            // Update the flags.
            pExr->ExceptionFlags = ExceptionFlags;
            ExceptionFlags &= ~(EXCEPTION_COLLIDED_UNWIND |
                                EXCEPTION_TARGET_UNWIND);

            // The handler must be executed by calling another routine
            // that is written in assembler. This is required because
            // up level addressing of the handler information is required
            // when a collided unwind is encountered.
            DEBUGMSG(DBGEH, (TEXT("Calling termination handler @%8.8lx RP=%8.8lx\r\n"),
                         pRP->Handler, pRP));

            Disposition = RtlpExecuteHandlerForUnwind(pExr, pRP, pCtx,
                            &DispatcherContext, pRP->Handler);

            DEBUGMSG(DBGEH, (TEXT("  disposition = %d\r\n"), Disposition));

            if (Disposition == ExceptionContinueSearch) {
                DEBUGMSG(DBGEH, (TEXT("RtlUnwind: disposition == ExceptionContinueSearch\r\n")));
                break;
            }

            if (Disposition != ExceptionCollidedUnwind) {
                RETAILMSG(1, (TEXT("RtlUnwind: invalid dispositon (%d)\r\n"),
                              Disposition));
                return FALSE;
            }

            DEBUGMSG(DBGEH, (TEXT("RtlUnwind: disposition == ExceptionCollidedUnwind\r\n")));
            ExceptionFlags |= EXCEPTION_COLLIDED_UNWIND;
            if (pRP == DispatcherContext.RegistrationPointer) {
                break;
            }

            // Pick up the registration pointer that was active at
            // the time of the unwind, and continue.
            pRP = DispatcherContext.RegistrationPointer;

        } while (continueLoop);

        // If this is the target of the unwind, then continue execution
        // by calling the continue system service.
        if (pRP == TargetFrame) {
            StoreRegistrationPointer(pRP);
            break;
        }

        // Step to next registration record and unlink
        // the unwind handler since it has been called.
        pRP = pRP->Next;
        StoreRegistrationPointer(pRP);

    }

    // If the registration pointer is equal to the target frame
    // pointer, then continue execution. Otherwise, an exit unwind was
    // performed or the target of the unwind did not exist and the
    // debugger and subsystem are given a second chance to handle the
    // unwind.
    if (TargetFrame && (pRP == TargetFrame)) {
        DEBUGMSG(DBGEH, (TEXT("RtlUnwind: continuing @%8.8lx\r\n"),
                CONTEXT_TO_PROGRAM_COUNTER(pCtx)));
        return TRUE;
    }
    return FALSE;
}



