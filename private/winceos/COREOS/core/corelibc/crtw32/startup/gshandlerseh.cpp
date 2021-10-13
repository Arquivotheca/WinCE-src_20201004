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
/***
*gshandlerseh.c - Defines __GSHandlerCheck_SEH
*
*Purpose:
*       Defines __GSHandlerCheck_SEH, the exception handler for functions
*       with /GS-protected buffers as well as SEH (__try) constructs.
*
*******************************************************************************/

#include <corecrt.h>

#if defined(_M_IX86)
#error x86 does not use __GSHandlerChecks
#endif

extern "C" {

void
__GSHandlerCheckCommon (
    IN PVOID EstablisherFrame,
    IN PDISPATCHER_CONTEXT DispatcherContext,
    IN PGS_HANDLER_DATA GSHandlerData
    );

EXCEPTION_DISPOSITION
__C_specific_handler (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext
    );

/***
*__GSHandlerCheck_SEH - Check local security cookie during SEH
*
*Purpose:
*   Functions which have a local security cookie, as well as SEH (__try), will
*   register this routine for exception handling.  It checks the validity of
*   the local security cookie during exception dispatching and unwinding.  This
*   helps defeat buffer overrun attacks that trigger an exception to avoid the
*   normal end-of-function cookie check.  If the security check succeeds,
*   control passes to __C_specific_handler to perform normal SEH processing.
*
*   Note that this routine must be statically linked into any module that uses
*   it, since it needs access to the global security cookie of that function's
*   image.
*
*Entry:
*   ExceptionRecord - Supplies a pointer to an exception record.
*   EstablisherFrame - Supplies a pointer to frame of the establisher function.
*   ContextRecord - Supplies a pointer to a context record.
*   DispatcherContext - Supplies a pointer to the exception dispatcher or
*       unwind dispatcher context.
*
*Return:
*   If the security cookie check fails, the process is terminated.  Otherwise,
*   return the result of __C_specific_handler.
*
*******************************************************************************/

EXCEPTION_DISPOSITION
__GSHandlerCheck_SEH (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext
    )
{
    PSCOPE_TABLE ScopeTable;
    PGS_HANDLER_DATA GSHandlerData;

    //
    // Retrieve a pointer to the start of that part of the handler data used
    // to locate the local security cookie in the local frame.  That is found
    // following __C_specific_handler's scope table.
    //

    ScopeTable = (PSCOPE_TABLE)(DispatcherContext->FunctionEntry->HandlerData);
    GSHandlerData = (PGS_HANDLER_DATA)&ScopeTable->ScopeRecord[ScopeTable->Count];

    //
    // Perform the actual cookie check.
    //

    __GSHandlerCheckCommon(
        EstablisherFrame,
        DispatcherContext,
        GSHandlerData
        );

    //
    // If the cookie check succeeds, call the normal SEH handler if we're
    // supposed to on this exception pass.  Find the EHANDLER/UHANDLER flags
    // controlling that in the first ULONG of our part of the handler data.
    //

    return __C_specific_handler(
                ExceptionRecord,
                EstablisherFrame,
                ContextRecord,
                DispatcherContext
                );
}

} // extern "C"
