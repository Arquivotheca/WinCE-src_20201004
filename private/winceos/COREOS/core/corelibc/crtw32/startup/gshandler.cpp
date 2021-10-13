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
*gshandler.c - Defines __GSHandlerCheck (non-x86)
*
*Purpose:
*       Defines __GSHandlerCheck, the exception handler for functions with
*       /GS-protected buffers but no SEH or C++ EH constructs.
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

/***
*__GSHandlerCheck - Check local security cookie during exception handling
*
*Purpose:
*   Functions which have a local security cookie, but do not use any exception
*   handling (either C++ EH or SEH), will register this routine for exception
*   handling.  It exists simply to check the validity of the local security
*   cookie during exception dispatching and unwinding.  This helps defeat
*   buffer overrun attacks that trigger an exception to avoid the normal
*   end-of-function cookie check.
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
*   return an exception disposition of continue execution.
*
*******************************************************************************/

EXCEPTION_DISPOSITION
__GSHandlerCheck (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext
    )
{
    //
    // The common helper performs all the real work.
    //

    __GSHandlerCheckCommon(
        EstablisherFrame,
        DispatcherContext,
        (PGS_HANDLER_DATA)DispatcherContext->FunctionEntry->HandlerData
        );

    //
    // The cookie check succeeded, so continue the search for exception or
    // termination handlers.
    //

    return ExceptionContinueSearch;
}

/***
*__GSHandlerCheckCommon - Helper for exception handlers checking the local
*                         security cookie during exception handling
*
*Purpose:
*   This performs the actual local security cookie check for the three
*   exception handlers __GSHandlerCheck, __GSHandlerCheck_SEH, and
*   __GSHandlerCheckEH.
*
*   Note that this routine must be statically linked into any module that uses
*   it, since it needs access to the global security cookie of that function's
*   image.
*
*Entry:
*   EstablisherFrame - Supplies a pointer to frame of the establisher function.
*   DispatcherContext - Supplies a pointer to the exception dispatcher or
*       unwind dispatcher context.
*   GSHandlerData - Supplies a pointer to the portion of the language-specific
*       handler data that is used to find the local security cookie in the
*       frame.
*
*Return:
*   If the security cookie check fails, the process is terminated.
*
*******************************************************************************/

void
__GSHandlerCheckCommon (
    IN PVOID EstablisherFrame,
    IN PDISPATCHER_CONTEXT DispatcherContext,
    IN PGS_HANDLER_DATA GSHandlerData
    )
{
    LONG CookieOffset;
    PCHAR CookieFrameBase;
    UINT_PTR Cookie;
    LONG_PTR AlignBase;

    //
    // Find the offset of the local security cookie within the establisher
    // function's call frame, from some as-yet undetermined base point.  The
    // bottom 2 bits of the cookie offset DWORD are used for other purposes,
    // so zero them out.
    //

    CookieOffset = GSHandlerData->u.CookieOffset & ~0x03;

    //
    // The base from which the cookie offset is applied is either the SP
    // within that frame, passed here as EstablisherFrame, or in the case of
    // dynamically-aligned frames, an aligned address offset from SP.
    //

    CookieFrameBase = (PCHAR)EstablisherFrame;
    if (GSHandlerData->u.Bits.HasAlignment)
    {
        AlignBase =
            (LONG_PTR)EstablisherFrame + GSHandlerData->AlignedBaseOffset;
        CookieFrameBase -= AlignBase - (AlignBase & -GSHandlerData->Alignment);
    }

    //
    // Retrieve the local security cookie, now that we have determined its
    // location.
    //

    Cookie = *(PUINT_PTR)(CookieFrameBase + CookieOffset);

    //
    // Send the cookie to __security_check_cookie for final validation and
    // possible fatal Watson dump.
    //

    __security_check_cookie(Cookie);
}

} // extern "C"
