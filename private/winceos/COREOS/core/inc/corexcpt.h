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

#pragma once

#ifndef _INC_COREXCPT
#define _INC_COREXCPT

// stack goes bottom up, the range is open on the low-end
FORCEINLINE BOOL IsValidStkPtr (DWORD x, DWORD dwLowLimit, DWORD dwHighLimit)
{
    return     (x > dwLowLimit)
            && (x <= dwHighLimit)
            && IsDwordAligned (x);
}

FORCEINLINE DWORD GetStkHighLimit (void)
{
    return UStkBase () + UStkSize () - 4 * REG_SIZE;
}

void RtlReportUnhandledException (
    IN PEXCEPTION_RECORD pExr,
    IN PCONTEXT pCtx
    );

EXCEPTION_DISPOSITION RtlpExecuteHandlerForException(
    IN PEXCEPTION_RECORD pExr,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT pCtx,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext
#ifdef x86
    , IN PEXCEPTION_ROUTINE ExceptionRoutine
#endif
    );

EXCEPTION_DISPOSITION RtlpExecuteHandlerForUnwind(
    IN PEXCEPTION_RECORD pExr,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT pCtx,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext
#ifdef x86
    , IN PEXCEPTION_ROUTINE ExceptionRoutine
#endif
    );

void RtlResumeFromContext (
    IN PCONTEXT pCtx);



void CallGlobalHandler (PCONTEXT pCtx, PEXCEPTION_RECORD pExr);
void CallVectorHandlers (PEXCEPTION_RECORD pExr, PCONTEXT pCtx);

#ifdef KCOREDLL
#define ExdReadPEHeader(unused, flags, addr, pbuf, sz) ReadKernelPEHeader (flags, addr, pbuf, sz)
#else
#define ExdReadPEHeader(h, flags, addr, pbuf, sz) ReadPEHeader (h, flags, addr, pbuf, sz)
#endif

#endif /* _INC_COREXCPT */
