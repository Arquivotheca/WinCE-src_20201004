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

