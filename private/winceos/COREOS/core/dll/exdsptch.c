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

//
// Format of PData
//
typedef struct PDATA {
    ULONG pFuncStart;
    ULONG PrologLen : 8;        // low order bits
    ULONG FuncLen   : 22;       // high order bits
    ULONG ThirtyTwoBits : 1;    // 0/1 == 16/32 bit instructions
    ULONG ExceptionFlag : 1;    // highest bit
} PDATA, *PPDATA;


//# Disable C4200: nonstandard extension used zero-sized array in structunion
#pragma warning( push )
#pragma warning ( disable : 4200 )

//
// format of external PData
//
typedef struct _pdata_tag {
    struct _pdata_tag *pNextTable;
    int     numentries;     // number of entries below
    PDATA   entry[];        // entries in-lined like in a .pdata section
} XPDATA, *LPXPDATA;

#pragma warning( pop )

// restore SP and raise an "Unhandle Exception"
static PEXCEPTION_ROUTINE g_pfnEH;

// extended PData
static LPXPDATA *g_pExtPData;

//
// API to set global exception handler
//
void
SetExceptionHandler(
    PEXCEPTION_ROUTINE  per
    )
{
    g_pfnEH = per;
}

//
// API to set extended PData
//

BOOL
CeSetExtendedPdata(
    LPVOID  pExtPData
    )
{
    g_pExtPData = (LPXPDATA *) pExtPData;
    return TRUE;
}

// stack goes bottom up, the range is open on the low-end
__inline BOOL IsValidStkPtr (DWORD x, DWORD dwLowLimit, DWORD dwHighLimit)
{
    return     (x > dwLowLimit)
            && (x <= dwHighLimit)
            && IsStkAligned (x);
}

DWORD GetStkHighLimit (void)
{
    return UStkBase () + UStkSize () - 4 * REG_SIZE;
}
//
// Define private function prototypes.
//
void RtlReportUnhandledException (
    IN PEXCEPTION_RECORD pExr,
    IN PCONTEXT pCtx
    );

EXCEPTION_DISPOSITION RtlpExecuteHandlerForException(
    IN PEXCEPTION_RECORD pExr,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT pCtx,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine);

EXCEPTION_DISPOSITION RtlpExecuteHandlerForUnwind(
    IN PEXCEPTION_RECORD pExr,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT pCtx,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine);

BOOLEAN RtlUnwind(
#ifdef x86
    IN PEXCEPTION_REGISTRATION_RECORD TargetFrame,
#else
    IN ULONG TargetFrame,
#endif
    IN ULONG TargetIp,
    IN PEXCEPTION_RECORD pExr,
    IN ULONG ReturnValue,
    IN PCONTEXT pCtx);

void RtlResumeFromContext (
    IN PCONTEXT pCtx);


static void CallGlobalHandler (PCONTEXT pCtx, PEXCEPTION_RECORD pExr)
{
    DISPATCHER_CONTEXT DispatcherContext = {0};

    DEBUGCHK (g_pfnEH);

#ifndef x86
    DispatcherContext.ContextRecord = pCtx;
#endif

    // clear "Unwinding" flag, we only call the global handler for dispatch.
    pExr->ExceptionFlags &= ~EXCEPTION_UNWINDING;

    // if the global excepiton handler return anything but ExceptionExecuteHandler, we would've failed
    // searching for exception handler and there is no need to contintue searching.
    if ((ExceptionExecuteHandler == (* g_pfnEH) (pExr, 0, pCtx, &DispatcherContext))
        && DispatcherContext.ControlPc) {
        CONTEXT_TO_RETVAL (pCtx) = (*((DWORD (*) (void)) DispatcherContext.ControlPc)) ();

//
// The following 2 statements will make the global handler a "catch-all" exception handler, instead
// of a "finally" handler. Since we don't know what value we should return, we need to force it into
// a global finally clause.
//
//        CONTEXT_TO_PROGRAM_COUNTER (pCtx) = SYSCALL_RETURN;
//        RtlResumeFromContext (pCtx);
    }
}

#ifndef KCOREDLL

DLIST            g_veList = {&g_veList, &g_veList};
CRITICAL_SECTION g_veCS;    // Critical section to protect vector handler list
DWORD            g_nVectors;

typedef struct _VECTORHANDLERS {
    DLIST                       link;           // DLIST
    PVECTORED_EXCEPTION_HANDLER pfnHandler;     // the handler
} VECTORHANDLERS, *PVECTORHANDLERS;

#define CV_RETRY        0xbeaf

ERRFALSE (EXCEPTION_CONTINUE_EXECUTION != CV_RETRY);
ERRFALSE (EXCEPTION_CONTINUE_SEARCH    != CV_RETRY);

#pragma warning(push)
#pragma warning(disable:4995 4996)
//
// in excepiton handler, we can't do heap allocation as heap can be corrupted and we'll get into infinite recursion.
// use _alloca here should be pretty safe as there ususally isn't a lot of Vector Handler registered.
//
static DWORD DoCallVectorHandlers (PEXCEPTION_POINTERS pEp, DWORD nVectors)
{
    DWORD dwRet = EXCEPTION_CONTINUE_SEARCH;
    PVECTORED_EXCEPTION_HANDLER *pveh;
    if (nVectors 
        && (NULL != (pveh = (PVECTORED_EXCEPTION_HANDLER *) _alloca (nVectors * sizeof (PVECTORED_EXCEPTION_HANDLER))))) {

        EnterCriticalSection (&g_veCS);

        if (nVectors < g_nVectors) {
            // a new vector added between alloca and we grabbed CS, retry
            dwRet = CV_RETRY;
        } else {

            PDLIST ptrav;
            nVectors = 0;
            for (ptrav = g_veList.pFwd; ptrav != &g_veList; ptrav = ptrav->pFwd, nVectors ++) {
                // Record the handler, for we don't want to call the handlers holding the vector-list
                // critical section. Or we can get into deadlock situation.
                // e.g. - one of the handler allocate memory, asking for heap CS. Then
                //      thread 1 - exception in heap code, hold heap CS, accquiring vector CS
                //      thread 2 - regular exception, hold vector CS, call vector handler and acquire heap CS.
                pveh[nVectors] = ((PVECTORHANDLERS) ptrav)->pfnHandler;
            }
        }
        LeaveCriticalSection (&g_veCS);

        if (CV_RETRY != dwRet) {
            DWORD idx;
            for (idx = 0; idx < nVectors; idx ++) {
                if ((dwRet = pveh[idx] (pEp)) == EXCEPTION_CONTINUE_EXECUTION) {
                    break;
                }
                // the vector is supposed to only return EXCEPTION_CONTINUE_SEARCH or EXCEPTION_CONTINUE_EXECUTION
                DEBUGCHK (EXCEPTION_CONTINUE_SEARCH == dwRet);
                dwRet = EXCEPTION_CONTINUE_SEARCH;
            }
        }
    }

    return dwRet;
}
#pragma warning(pop)

// C4204: nonstandard extension used : non-constant aggregate initializer
#pragma warning(push)
#pragma warning(disable:4204)

static void CallVectorHandlers (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    // don't call vector handler if stack overflow.
    if (STATUS_STACK_OVERFLOW != pExr->ExceptionCode) {
        EXCEPTION_POINTERS ep = { pExr, pCtx };
        DWORD dwAction;

        do {
            dwAction = DoCallVectorHandlers (&ep, g_nVectors);
            if (EXCEPTION_CONTINUE_EXECUTION == dwAction) {
                RtlResumeFromContext (pCtx);
                // should never return
                DEBUGCHK (0);
            }

        } while (CV_RETRY == dwAction);
    }
}

#pragma warning(pop)

//
// Vector exception handler support
//
WINBASEAPI
PVOID
WINAPI
AddVectoredExceptionHandler(
    IN ULONG FirstHandler,
    IN PVECTORED_EXCEPTION_HANDLER VectoredHandler
    )
{
    if (!VectoredHandler) {
        return NULL;
    } else {
        PVECTORHANDLERS pvh = LocalAlloc (0, sizeof (VECTORHANDLERS));
        if (pvh) {
            pvh->pfnHandler = VectoredHandler;

            EnterCriticalSection (&g_veCS);
            if (FirstHandler) {
                AddToDListHead (&g_veList, &pvh->link);
            } else {
                AddToDListTail (&g_veList, &pvh->link);
            }
            g_nVectors ++;
            LeaveCriticalSection (&g_veCS);
        }
        return pvh;
    }
}

WINBASEAPI
ULONG
WINAPI
RemoveVectoredExceptionHandler(
    IN PVOID VectoredHandlerHandle
    )
{
    if (!VectoredHandlerHandle) {
        return 0;
    } else {
        PDLIST ptrav;
        EnterCriticalSection (&g_veCS);
        for (ptrav = g_veList.pFwd; ptrav != &g_veList; ptrav = ptrav->pFwd) {
            if (ptrav == (PDLIST) VectoredHandlerHandle) {
                RemoveDList (ptrav);
                g_nVectors --;
                break;
            }
        }
        LeaveCriticalSection (&g_veCS);

        if (&g_veList != ptrav) {
            // found
            LocalFree (ptrav);
        }
        return (&g_veList != ptrav);
    }
}

#define ExdReadPEHeader(h, flags, addr, pbuf, sz) ReadPEHeader (h, flags, addr, pbuf, sz)

#ifdef x86
extern BOOL g_fDepEnabled;
#endif

#else //KCOREDLL

// KCOREDLL - vector handlers not supported
static void CallVectorHandlers (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
}

#define ExdReadPEHeader(unused, flags, addr, pbuf, sz) ReadKernelPEHeader (flags, addr, pbuf, sz)

#endif



#ifdef x86

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

        /* See if the handler is reasonable
         */

        if (!RtlIsValidHandler (pRP->Handler)) {
            DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: Dispatch blocked")));
            RtlInvalidHandlerDetected ((PVOID)(LONG)(pRP->Handler), pExr, pOrigCtx);
            pExr->ExceptionFlags |= EXCEPTION_STACK_INVALID;
            RtlReportUnhandledException (pExr, pOrigCtx);
        }

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
        if (g_pfnEH) {
            *pCtx = *pOrigCtx;
            CallGlobalHandler (pCtx, pExr);
        }

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


#else

//
// Define private function prototypes.
//

ULONG
RtlpVirtualUnwind(
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN PCONTEXT pCtx,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame,
    IN e32_lite *eptr);

ULONG
RtlVirtualUnwind (
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN OUT PCONTEXT ContextRecord,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame,
    IN e32_lite *eptr           // eptr of the module where ControlPc is
    );

PRUNTIME_FUNCTION
RtlLookupFunctionEntry (
    IN DWORD  FunctionTableAddr,
    IN DWORD  SizeOfFunctionTable,
    IN ULONG  ControlPc,
    OUT PRUNTIME_FUNCTION prf
    );


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
BOOL
MatchesExtRange(
    ULONG pc,
    LPXPDATA pPData
    )
{
    ULONG start = 0, end = 0;

    if (pPData->numentries) {
        start = pPData->entry[0].pFuncStart;
        end   = pPData->entry[pPData->numentries-1].pFuncStart
              + pPData->entry[pPData->numentries-1].FuncLen;
    }
    return (pc >= start) && (pc < end);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD LookupPDataFormExt (
    ULONG pc,
    PULONG pSize
    )
{
    PVOID pResult = 0;
    LPXPDATA pPData;

    if (g_pExtPData) {
        for (pPData = *g_pExtPData; pPData; pPData = pPData->pNextTable) {
            if (MatchesExtRange (pc, pPData)) {
                pResult = &pPData->entry[0];
                *pSize = pPData->numentries * sizeof(pPData->entry[0]);
                break;
            }
        }
    }

    return (DWORD) pResult;
}

//------------------------------------------------------------------------------
//
// helper function to perform exception handling, return ExceptionFlags, if ever
//
//------------------------------------------------------------------------------
DWORD RtlDispatchHelper (
    IN PEXCEPTION_RECORD pExr,
    IN OUT PCONTEXT pCtx,
    IN PCONTEXT pOrigCtx,
    IN DWORD ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN DWORD EstablisherFrame,
    IN OUT PULONG pNestedFrame,
    IN DWORD ExceptionFlags
)
{
    DISPATCHER_CONTEXT DispatcherContext;
    EXCEPTION_DISPOSITION Disposition;

    // The frame has an exception handler. The handler must be
    // executed by calling another routine that is written in
    // assembler. This is required because up level addressing
    // of the handler information is required when a nested
    // exception is encountered.

    DispatcherContext.ControlPc = ControlPc;
    DispatcherContext.FunctionEntry = FunctionEntry;
    DispatcherContext.EstablisherFrame = EstablisherFrame;
    DispatcherContext.ContextRecord = pCtx;

    pExr->ExceptionFlags = ExceptionFlags;
    DEBUGMSG(DBGEH, (TEXT("Calling exception handler @%8.8lx(%8.8lx) Frame=%8.8lx\r\n"),
            FunctionEntry->ExceptionHandler, FunctionEntry->HandlerData,
            EstablisherFrame));

    Disposition = RtlpExecuteHandlerForException(pExr,
            (LPVOID) EstablisherFrame, pCtx, &DispatcherContext,
            FunctionEntry->ExceptionHandler);

    ExceptionFlags |= (pExr->ExceptionFlags & EXCEPTION_NONCONTINUABLE);

    DEBUGMSG(DBGEH, (TEXT("exception dispostion = %d\r\n"), Disposition));

    // If the current scan is within a nested context and the frame
    // just examined is the end of the nested region, then clear
    // the nested context frame and the nested exception flag in
    // the exception flags.
    if (*pNestedFrame == EstablisherFrame) {
        ExceptionFlags &= (~EXCEPTION_NESTED_CALL);
        *pNestedFrame = 0;
    }

    // Case on the handler disposition.
    switch (Disposition) {

    // The disposition is to execute a frame based handler. The
    // target handler address is in DispatcherContext.ControlPc.
    // Reset to the original context and switch to unwind mode.
    case ExceptionExecuteHandler:
        DEBUGMSG(DBGEH, (TEXT("Unwinding to %8.8lx Frame=%8.8lx\r\n"),
                DispatcherContext.ControlPc, EstablisherFrame));

        if (RtlUnwind (EstablisherFrame, DispatcherContext.ControlPc,
                        pExr, pExr->ExceptionCode, pCtx)) {
            // continue execution from where Context record specifies
            DEBUGMSG(DBGEH, (TEXT("Continuing to Exception Handler at %8.8lx...\r\n"), CONTEXT_TO_PROGRAM_COUNTER (pCtx)));
            RtlResumeFromContext (pCtx);
        }

        // set non-continuable flag, such that we'll report unhandled exception
        // after fall through the case statement
        pExr->ExceptionFlags |= EXCEPTION_NONCONTINUABLE;
        // fall through to either continue or report unhandledexception

    // The disposition is to continue execution.
    // If the exception is not continuable, then raise the
    // exception STATUS_NONCONTINUABLE_EXCEPTION. Otherwise
    // return exception handled.
    case ExceptionContinueExecution:

        if (pExr->ExceptionFlags & EXCEPTION_NONCONTINUABLE) {
            // try to continue on a non-continuable exception
            DEBUGMSG(DBGEH, (L"Reporting 2nd-chance Exception\r\n"));
            RtlReportUnhandledException (pExr, pOrigCtx);
        } else {
            // continue execution from where Context record specifies
            DEBUGMSG(DBGEH, (TEXT("Continuing execution at %8.8lx...\r\n"), CONTEXT_TO_PROGRAM_COUNTER (pCtx)));
            RtlResumeFromContext (pCtx);
        }
        // both of the above should not return at all
        DEBUGCHK (0);
        break;

    // The disposition is to continue the search.
    // Get next frame address and continue the search.
    case ExceptionContinueSearch :
        break;

    // The disposition is nested exception.
    // Set the nested context frame to the establisher frame
    // address and set the nested exception flag in the
    // exception flags.
    case ExceptionNestedException :
        DEBUGMSG(DBGEH, (TEXT("Nested exception: frame=%8.8lx\r\n"),
                DispatcherContext.EstablisherFrame));
        ExceptionFlags |= EXCEPTION_NESTED_CALL;
        if (DispatcherContext.EstablisherFrame > *pNestedFrame)
            *pNestedFrame = DispatcherContext.EstablisherFrame;
        break;

    // All other disposition values are invalid.
    // Raise invalid disposition exception.
    default :
        RETAILMSG(1, (TEXT("RtlDispatchException: invalid dispositon (%d)\r\n"),
                Disposition));
        RtlReportUnhandledException (pExr, pOrigCtx);
    }

    return ExceptionFlags;

}

__inline BOOL IsAddrInModule (e32_lite *eptr, DWORD dwAddr)
{
    return (DWORD) (dwAddr - eptr->e32_vbase) < eptr->e32_vsize;
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
    pth - Supplies a pointer to thread structure
    pExr - Supplies a pointer to an exception record.
    pCtx - Supplies a pointer to a context record.

Return Value:
    If the exception is handled by one of the frame based handlers, then
    a value of TRUE is returned. Otherwise a value of FALSE is returned.
--*/
{
    CONTEXT LocalCtx = *pCtx;       // Local Context used in actual unwinding
    CONTEXT VirtualCtx = *pCtx;     // Virtual Context used for VirtualUnwind
    PCONTEXT pOrigCtx = pCtx;       // orignal context, for unhandled exception reporting
    PCONTEXT pLocalCtx = &LocalCtx; // pointer to LocalCtx
    ULONG ControlPc;
    ULONG EstablisherFrame;
    ULONG ExceptionFlags;
    PRUNTIME_FUNCTION FunctionEntry;
    BOOLEAN InFunction;
    const UINT LowLimit = UStkBound ();
    const UINT HighLimit = GetStkHighLimit ();
    ULONG NestedFrame;
    ULONG NextPc;
    DWORD PrevSP;
    RUNTIME_FUNCTION TempFunctionEntry;
    e32_lite e32 = {0};
    DWORD dwPDataBase, dwPDataSize;

    pExr->ExceptionFlags &= ~EXCEPTION_FLAG_IN_CALLBACK;

    // call vector exceptoin handlers
    // NOTE: CallVectorHandler does NOT return if Vector handlers take care of exception.
    CallVectorHandlers (pExr, pCtx);

    //
    // Redirect pCtx to a local copy of the incoming context. The
    // exception dispatch process modifies the PC of this context. Unwinding
    // of nested exceptions will fail when unwinding through CaptureContext
    // with a modified PC.
    //

    // throughout the function
    //
    //      pCtx: points to the local copy of the context. Used in VirtualUnwind to determine termination situation
    //      pLocalCtx: points to the local copy of original context, used when invoking filter and final unwind.
    //
    pCtx = &VirtualCtx;

    ControlPc = CONTEXT_TO_PROGRAM_COUNTER (pCtx);
    ExceptionFlags = pExr->ExceptionFlags & EXCEPTION_NONCONTINUABLE;
    NestedFrame = 0;

    // Start with the frame specified by the context record and search
    // backwards through the call frame hierarchy attempting to find an
    // exception handler that will handle the exception.
    do {
        // Lookup the function table entry using the point at which control
        // left the procedure.
        DEBUGMSG(DBGEH, (TEXT("RtlDispatchException(%08x): ControlPc=%8.8lx SP=%8.8lx, PSR = %8.8lx\r\n"),
                pCtx, ControlPc, pCtx->IntSp, pCtx->Psr));

        PrevSP = (DWORD) pCtx->IntSp;

        // Lookup the function table entry using the point at which control left the procedure.
        dwPDataBase = 0;
        if (IsAddrInModule (&e32, ControlPc)
            || ExdReadPEHeader (hActiveProc, 0, ControlPc, &e32, sizeof (e32))) {
            dwPDataBase = e32.e32_vbase+e32.e32_unit[EXC].rva;
            dwPDataSize = e32.e32_unit[EXC].size;
        } else {
            dwPDataBase = LookupPDataFormExt (ControlPc, &dwPDataSize);
        }

        FunctionEntry = dwPDataBase
            ? RtlLookupFunctionEntry (dwPDataBase, dwPDataSize, ControlPc, &TempFunctionEntry)
            : NULL;

        // If there is a function table entry for the routine, then virtually
        // unwind to the caller of the current routine to obtain the virtual
        // frame pointer of the establisher and check if there is an exception
        // handler for the frame.
        if (FunctionEntry) {

            NextPc = RtlVirtualUnwind (ControlPc,
                                      FunctionEntry,
                                      pCtx,
                                      &InFunction,
                                      &EstablisherFrame,
                                      &e32);

            DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: RtlVirtualUnwind returns, InFunction=%8.8lx EstablisherFrame=%8.8lx, NextPC = %8.8lx\r\n"),
                InFunction, EstablisherFrame, NextPc));

            // If the virtual frame pointer is not within the specified stack
            // limits or the virtual frame pointer is unaligned, then set the
            // stack invalid flag in the exception record and return exception
            // not handled. Otherwise, check if the current routine has an
            // exception handler.
            if (!IsValidStkPtr (EstablisherFrame, LowLimit, HighLimit)) {
                break;
            }

            if (InFunction && FunctionEntry->ExceptionHandler) {

                ExceptionFlags = RtlDispatchHelper (pExr, pLocalCtx, pOrigCtx, ControlPc, FunctionEntry,
                                            EstablisherFrame, &NestedFrame, ExceptionFlags);

            } else if (NextPc != ControlPc) {

                PrevSP = 0; // don't terminate loop because sp didn't change.
            }

        } else {
            // Set point at which control left the previous routine.
            NextPc = (ULONG) pCtx->IntRa - INST_SIZE;
            PrevSP = 0; // don't terminate loop because sp didn't change.

            // If the next control PC is the same as the old control PC, then
            // the function table is not correctly formed.
            if (NextPc == ControlPc)
                break;
        }

        // Set point at which control left the previous routine.
        ControlPc = NextPc;
        if (IsPSLBoundary (ControlPc)) {

            // unhandled exception reached PSL boundary
            break;
        }
    } while ((ULONG) pCtx->IntSp < HighLimit && (ULONG) pCtx->IntSp > PrevSP);


    if (IsPSLBoundary (ControlPc)) {
        DEBUGMSG(DBGEH, (TEXT("RtlDispatchException: Handler not found, perform Exit-Unwind pc=%x code=%x\n"),
            CONTEXT_TO_PROGRAM_COUNTER (pCtx), pExr->ExceptionCode));

        // perform an EXIT_UNWIND if cross PSL boundary
        RtlUnwind (0, 0, pExr, pExr->ExceptionCode, pLocalCtx);

        // try global handler
        if (g_pfnEH) {

            *pCtx = *pOrigCtx;
            CallGlobalHandler (pCtx, pExr);
        }

    }

    pExr->ExceptionFlags = ExceptionFlags;

    // Set final exception flags and return exception not handled.
    DEBUGMSG(1, (TEXT("RtlDispatchException: returning failure. Flags=%x\r\n"), ExceptionFlags));

    RtlReportUnhandledException (pExr, pOrigCtx);
    // should never return
    DEBUGCHK (0);
}

#if defined(COMPRESSED_PDATA)


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PRUNTIME_FUNCTION
RtlLookupFunctionEntry(
    IN  DWORD  FunctionTableAddr,
    IN  DWORD  SizeOfFunctionTable,
    IN  ULONG  ControlPc,
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
    PPDATA table = (PPDATA) FunctionTableAddr;
    PPDATA entry;
    PPDATA_EH peh;
    ULONG InstSize;
    LONG High;
    LONG Low;
    LONG Middle;
    DWORD dwSlot = 0; //FunctionTableAddr - ZeroPtr (FunctionTableAddr);

    if (!table || (SizeOfFunctionTable < sizeof (PDATA))) {
        return NULL;
    }

    // controlPC must be zero based.
    //DEBUGCHK (ZeroPtr (ControlPc) == ControlPc);

    __try {

        // Search the function table for a function table entry for the specified PC.

        // Initialize search indicies.
        Low = 0;
        High = (SizeOfFunctionTable / sizeof(PDATA)) - 1;

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
#ifdef ARM
                if (entry->PrologLen == 0xFF) {
                    // Pogo-BBR PDATA entry: this is a basic block which has been
                    // separated from main function. We need to point to the PDATA
                    // of the block containing the Prologue
                    InstSize = entry->ThirtyTwoBits ? 4 : 2;
                    entry = *(PPDATA*)(entry->pFuncStart + entry->FuncLen * InstSize);
                }
#endif

                // The ControlPc is between the middle entry and the middle+1 entry
                // or this is the last entry that will be examined. Check ControlPc
                // against the function length.
                InstSize = entry->ThirtyTwoBits ? 4 : 2;
                prf->BeginAddress = entry->pFuncStart;
                prf->EndAddress = entry->pFuncStart + entry->FuncLen*InstSize;
                prf->PrologEndAddress = entry->pFuncStart + entry->PrologLen*InstSize;

                // Make sure this function entry covers ControlPc. Note that the
                // odd-ness of some PDATA fixups can cause overlapping PDATA and
                // confuse this search.  For example, say we have a Thumb function
                // (an pFuncStart), immediately followed by an ARM function with
                // *no* PDATA.  Then, say an exception occurs on the first
                // instruction in the ARM function.  In that case, ControlPc
                // will be N and the Thumb function's EndAddress will be N+1, so
                // this search will match the Thumb function's PDATA when it
                // should fail to find any PDATA at all.
                if (ControlPc >= (prf->EndAddress & ~1))
                    break;  // Not a match, stop searching

                // Fill in the remaining fields in the RUNTIME_FUNCTION structure.
                if (entry->ExceptionFlag) {
                    peh = (PPDATA_EH) ((entry->pFuncStart & ~(InstSize-1)) + dwSlot) - 1;
                    prf->ExceptionHandler = peh->pHandler;
                    prf->HandlerData = peh->pHandlerData;
                } else {
                    prf->ExceptionHandler = 0;
                    prf->HandlerData = 0;
                }

                return prf;
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
RtlLookupFunctionEntry (
    IN DWORD  FunctionTableAddr,
    IN DWORD  SizeOfFunctionTable,
    IN ULONG  ControlPc,
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
    PCOMBINED_HEADER Header = (PCOMBINED_HEADER) FunctionTableAddr;
    PRUNTIME_FUNCTION OldEntry;
    PRUNTIME_FUNCTION OldTable;
    PPDATA NewEntry;
    PPDATA NewTable;
    PPDATA_EH NewEH;
    ULONG EndAddress;
    LONG NewHigh = 0;
    LONG OldHigh;
    LONG Low;
    LONG Middle;
    UINT InstrShift;
    DWORD dwSlot = 0; //FunctionTableAddr - ZeroPtr (FunctionTableAddr);

    // If the exception table doesn't exist or the size is invalid, just return
    if (!Header || (SizeOfFunctionTable < sizeof (COMBINED_HEADER))) {
        return NULL;
    }

    // controlPC must be zero based.
    //DEBUGCHK (ZeroPtr (ControlPc) == ControlPc);

    __try {

        // If the PDATA is all old style then the header will actually be the
        // first uncompressed entry and the zero fields will be non-zero
        if (Header->zero1 || Header->zero2) {
            OldTable = (PRUNTIME_FUNCTION)Header;
            OldHigh = (SizeOfFunctionTable / sizeof(RUNTIME_FUNCTION)) - 1;
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
            // is greater than or equal to the beginning address and less
            // than the ending address of the function table entry, then
            // return the address of the function table entry. Otherwise,
            // continue the search.
            Middle = (Low + OldHigh) >> 1;
            OldEntry = &OldTable[Middle];

            if (ControlPc < OldEntry->BeginAddress) {
                OldHigh = Middle - 1;

            // See comment in the "COMPRESSED_PDATA" version of this function
            // above for why we mask off the LSb of EndAddress
            } else if (ControlPc >= (OldEntry->EndAddress & ~1)) {
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

                // See comment in the "COMPRESSED_PDATA" version of this function
                // above for why we mask off the LSb of EndAddress
                } else if (ControlPc >= (EndAddress & ~1)) {
                    Low = Middle + 1;

                } else {
                    prf->BeginAddress = NewEntry->pFuncStart;
                    prf->EndAddress = EndAddress;
                    prf->PrologEndAddress = NewEntry->pFuncStart + (NewEntry->PrologLen << InstrShift);

                    if (NewEntry->ExceptionFlag) {
                        NewEH = (PPDATA_EH) (NewEntry->pFuncStart + dwSlot) - 1;
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
RtlUnwind (
    IN ULONG TargetFrame,
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

    pExr - Supplies an optional pointer to an exception record.

    ReturnValue - Supplies a value that is to be placed in the integer
        function return register just before continuing execution.

    pCtx - Supplies a pointer to a context record that can be used
        to store context druing the unwind operation.

Return Value:
    None.
--*/
{
    ULONG ControlPc;
    DISPATCHER_CONTEXT DispatcherContext;
    PCONTEXT pOrigCtx;
    EXCEPTION_DISPOSITION Disposition;
    ULONG EstablisherFrame;
    ULONG ExceptionFlags;
    PRUNTIME_FUNCTION FunctionEntry;
    BOOLEAN InFunction;
    const UINT LowLimit = UStkBound ();
    const UINT HighLimit = GetStkHighLimit ();
    ULONG NextPc;
    RUNTIME_FUNCTION TempFunctionEntry;
    e32_lite e32 = {0};
    DWORD dwPDataBase, dwPDataSize;

    ControlPc = CONTEXT_TO_PROGRAM_COUNTER(pCtx);
    CONTEXT_TO_PROGRAM_COUNTER(pCtx) = (ULONG)TargetIp;
    EstablisherFrame = (ULONG) pCtx->IntSp;
    pOrigCtx = pCtx;

    // If the target frame of the unwind is specified, then a normal unwind
    // is being performed. Otherwise, an exit unwind is being performed.

    ExceptionFlags = EXCEPTION_UNWINDING;

    if (!TargetFrame)
        ExceptionFlags |= EXCEPTION_EXIT_UNWIND;

    // Scan backward through the call frame hierarchy and call exception
    // handlers until the target frame of the unwind is reached.
    do {
        DEBUGMSG(DBGEH, (TEXT("RtlUnwind(%08X): ControlPc=%8.8lx SP=%8.8lx\r\n"),
                pCtx, ControlPc, pCtx->IntSp));

        if (IsPSLBoundary (ControlPc)) {

            DEBUGMSG (DBGEH, (L"PSL Boundary, ControlPc = %8.8lx, TargetFrame = %8.8lx, TargetIp = %8.8lx\r\n", ControlPc, TargetFrame, TargetIp));
            if (TargetIp && !TargetFrame) {
                // The global exception handler return ExceptionExecuteHandler.
                return TRUE;
            }
            break;
        }

        dwPDataBase = 0;
        if (IsAddrInModule (&e32, ControlPc)
            || ExdReadPEHeader (hActiveProc, 0, ControlPc, &e32, sizeof (e32))) {
            dwPDataBase = e32.e32_vbase+e32.e32_unit[EXC].rva;
            dwPDataSize = e32.e32_unit[EXC].size;
        } else {
            dwPDataBase = LookupPDataFormExt (ControlPc, &dwPDataSize);
        }

        FunctionEntry = dwPDataBase
            ? RtlLookupFunctionEntry (dwPDataBase, dwPDataSize, ControlPc, &TempFunctionEntry)
            : NULL;

        // If there is a function table entry for the routine, then virtually
        // unwind to the caller of the routine to obtain the virtual frame
        // pointer of the establisher, but don't update the context record.
        if (FunctionEntry) {
            NextPc = RtlpVirtualUnwind(ControlPc,
                                       FunctionEntry,
                                       pCtx,
                                       &InFunction,
                                       &EstablisherFrame,
                                       &e32);

            // If the virtual frame pointer is not within the specified stack
            // limits, the virtual frame pointer is unaligned, or the target
            // frame is below the virtual frame and an exit unwind is not being
            // performed, then raise the exception STATUS_BAD_STACK. Otherwise,
            // check to determine if the current routine has an exception
            // handler.
            if (!IsValidStkPtr (EstablisherFrame, LowLimit, HighLimit)) {
                break;
            }

            if (InFunction && FunctionEntry->ExceptionHandler) {
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
                DispatcherContext.ContextRecord = pCtx;

                // Call the exception handler.
                do {
                    // If the establisher frame is the target of the unwind
                    // operation, then set the target unwind flag.
                    if ((ULONG)TargetFrame == EstablisherFrame)
                        ExceptionFlags |= EXCEPTION_TARGET_UNWIND;
                    pExr->ExceptionFlags = ExceptionFlags;

                    // Set the specified return value in case the exception
                    // handler directly continues execution.
                    CONTEXT_TO_RETVAL(pCtx) = (ULONG)ReturnValue;
                    DEBUGMSG(DBGEH, (TEXT("Calling unwind handler @%8.8lx Frame=%8.8lx\r\n"),
                            FunctionEntry->ExceptionHandler, EstablisherFrame));

                    Disposition = RtlpExecuteHandlerForUnwind(pExr,
                                                    (LPVOID) EstablisherFrame,
                                                    pCtx,
                                                    &DispatcherContext,
                                                    FunctionEntry->ExceptionHandler);

                    // don't need to free pcstkForHandler since it'll be freed at callback return
                    DEBUGMSG(DBGEH, (TEXT("  disposition = %d\r\n"), Disposition));

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
                            DEBUGMSG(DBGEH, (TEXT("RtlUnwind: Calling RtlVirtualUnwind %8.8lx, %8.8lx, %8.8lx\r\n"),
                                ControlPc, FunctionEntry, EstablisherFrame));
                            NextPc = RtlVirtualUnwind(ControlPc,
                                                      FunctionEntry,
                                                      pCtx,
                                                      &InFunction,
                                                      &EstablisherFrame,
                                                      &e32);
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
                        pCtx = DispatcherContext.ContextRecord;
                        CONTEXT_TO_PROGRAM_COUNTER(pCtx) = (ULONG)TargetIp;
                        ExceptionFlags |= EXCEPTION_COLLIDED_UNWIND;
                        EstablisherFrame = DispatcherContext.EstablisherFrame;
                        break;

                        // All other disposition values are invalid.
                        // Raise invalid disposition exception.
                    default :
                        return FALSE;
                    }
                } while (ExceptionFlags & EXCEPTION_COLLIDED_UNWIND);

                DEBUGMSG(DBGEH, (TEXT("RtlUnwind: NextPc =  %8.8lx\r\n"), NextPc));
            } else {
                // If the target frame has not been reached, then virtually unwind to the
                // caller of the current routine and update the context record.
                if (EstablisherFrame != (ULONG)TargetFrame) {
                    NextPc = RtlVirtualUnwind(ControlPc,
                                              FunctionEntry,
                                              pCtx,
                                              &InFunction,
                                              &EstablisherFrame,
                                              &e32);
                }
            }
        } else {
            // Set point at which control left the previous routine.
            NextPc = (ULONG) pCtx->IntRa - INST_SIZE;

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
    if (TargetFrame && (EstablisherFrame == (ULONG)TargetFrame)) {

        if ( pCtx != pOrigCtx ){        // collided unwind
            //
            //  A collided unwind ocurred and unwinding proceeded
            //  with the collided context. Copy the collided context
            //  back into the original context structure
            //
            memcpy(pOrigCtx, pCtx, sizeof(CONTEXT));
            pCtx = pOrigCtx;
        }

        CONTEXT_TO_RETVAL (pCtx) = (ULONG)ReturnValue;
        DEBUGMSG(DBGEH, (TEXT("RtlUnwind: continuing @%8.8lx\r\n"), CONTEXT_TO_PROGRAM_COUNTER(pCtx)));
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
    IN PCONTEXT pCtx,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame,
    IN e32_lite *eptr
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

    pCtx - Supplies the address of a context record.

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

    memcpy(&LocalContext, pCtx, sizeof(CONTEXT));
    return RtlVirtualUnwind(ControlPc,
                            FunctionEntry,
                            &LocalContext,
                            InFunction,
                            EstablisherFrame,
                            eptr);
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
        BOOLEAN fDummy;
        DWORD   dwDummy;
        RUNTIME_FUNCTION rf;
        PRUNTIME_FUNCTION prf = RtlLookupFunctionEntry (
                    eptr->e32_vbase+eptr->e32_unit[EXC].rva,
                    eptr->e32_unit[EXC].size,
                    ControlPc,
                    &rf);

        RtnAddr = prf
                ? RtlVirtualUnwind (ControlPc,
                            prf,
                            pCtx,
                            &fDummy,
                            &dwDummy,
                            eptr)
                : 0;
    }

    return RtnAddr;
}

#endif // KCOREDLL

#endif // x86

