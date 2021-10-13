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
/*++

Module Name:
    
    targexcept.cpp

Module Description:

    Redirect exception handler functions to actual ones.

--*/
#include "osaxs_p.h"

#if defined(x86)
extern "C" EXCEPTION_DISPOSITION __cdecl  
_except_handler3
(
    PEXCEPTION_RECORD XRecord,
    void *Registration,
    PCONTEXT Context,
    PDISPATCHER_CONTEXT Dispatcher
)
{
    return g_OsaxsData.p_except_handler3(XRecord, Registration, Context, Dispatcher);
}

extern "C" BOOL __abnormal_termination(void)
{
    return g_OsaxsData.p__abnormal_termination();
}

#else

extern "C" EXCEPTION_DISPOSITION __C_specific_handler
(
    PEXCEPTION_RECORD ExceptionRecord,
    PVOID EstablisherFrame, 
    PCONTEXT ContextRecord, 
    PDISPATCHER_CONTEXT DispatcherContext
)
{
    return g_OsaxsData.p__C_specific_handler(ExceptionRecord, EstablisherFrame, ContextRecord, DispatcherContext);
}

#endif

