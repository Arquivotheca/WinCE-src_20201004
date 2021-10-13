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
#include <excpt.h>
#include <corecrt.h>
#include <cruntime.h>
#include <ehdata.h>     // Declarations of all types used for EH
#include <trnsctrl.h>   // Routines to handle transfer of control
#include <ehstate.h>    // Declarations of state management stuff
#include <exception>    // User-visible routines for eh

static int __cdecl _CxxUnHandledExceptionFilter(EXCEPTION_POINTERS* pPtrs)
{
    if (PER_IS_MSVC_EH((EHExceptionRecord*)(pPtrs->ExceptionRecord))
     && !__crtIsDebuggerPresent())
    {
        std::terminate();            // Does not return
        return (EXCEPTION_EXECUTE_HANDLER);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}


////////////////////////////////////////////////////////////////////////////////
//
// UnhandledExceptionFilter - called by CRT startup if an exception reaches its
//                            filter
//
extern "C" int __cdecl UnhandledExceptionFilter(EXCEPTION_POINTERS* pPtrs)
{
  return _CxxUnHandledExceptionFilter(pPtrs);
}

