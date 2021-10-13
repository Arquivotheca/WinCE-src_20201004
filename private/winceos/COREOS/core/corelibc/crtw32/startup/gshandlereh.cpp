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
*gshandlereh.c - Defines __GSHandlerCheck_EH
*
*Purpose:
*       Defines __GSHandlerCheck_EH, the exception handler for functions
*       with /GS-protected buffers as well as C++ EH constructs.
*
*******************************************************************************/

#include <corecrt.h>
#include <ehdata.h>

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
__CxxFrameHandler3 (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext
    );

/***
*__GSHandlerCheck_EH - Check local security cookie during C++ EH
*
*Purpose:
*   Functions which have a local security cookie, as well as C++ EH, will
*   register this routine for exception handling.  It checks the validity of
*   the local security cookie during exception dispatching and unwinding.  This
*   helps defeat buffer overrun attacks that trigger an exception to avoid the
*   normal end-of-function cookie check.  If the security check succeeds,
*   control passes to __CxxFrameHandler3 to perform normal C++ EH processing.
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
*   return the result of __CxxFrameHandler3.
*
*******************************************************************************/

EXCEPTION_DISPOSITION
__GSHandlerCheck_EH (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext
    )
{
    PGS_HANDLER_DATA GSHandlerData;
    FuncInfo * EHFuncInfo;
#if defined(_M_ARM)
    GS_HANDLER_DATA ProxyGSHandlerData;
#endif

    //
    // Retrieve a pointer to the start of that part of the handler data used
    // to locate the local security cookie in the local frame.  That is found
    // following the FuncInfo table used by __CxxFrameHandler3.
    //

    EHFuncInfo = (FuncInfo *)DispatcherContext->FunctionEntry->HandlerData;

#if defined(_M_ARM)
    //
    // The ARM FuncInfo structure may have alignment information regardless
    // of whether the gs_cookie_offset is there.  Rearrange the data into
    // temporary storage so the format matches what the common handler is
    // expecting.
    //

    if (EHFuncInfo->EHFlags & FI_DYNSTKALIGN_FLAG)
    {
        ProxyGSHandlerData.u.CookieOffset = EHFuncInfo->gs_cookie_offset;
        ProxyGSHandlerData.u.Bits.HasAlignment = 1;
        ProxyGSHandlerData.AlignedBaseOffset = EHFuncInfo->offsetAlignStack;
        ProxyGSHandlerData.Alignment = EHFuncInfo->alignStack;
    }   
    else
    {
        ProxyGSHandlerData.u.CookieOffset = EHFuncInfo->gs_cookie_offset_noalign;
    }

    GSHandlerData = &ProxyGSHandlerData;
#else
    GSHandlerData = (PGS_HANDLER_DATA)&EHFuncInfo->gs_cookie_offset;
#endif

    //
    // Perform the actual cookie check.
    //

    __GSHandlerCheckCommon(
        EstablisherFrame,
        DispatcherContext,
        GSHandlerData
        );

    //
    // If the cookie check succeeds, call the normal C++ EH handler
    //

    return __CxxFrameHandler3(
                ExceptionRecord,
                EstablisherFrame,
                ContextRecord,
                DispatcherContext
                );
}

} // extern "C"
