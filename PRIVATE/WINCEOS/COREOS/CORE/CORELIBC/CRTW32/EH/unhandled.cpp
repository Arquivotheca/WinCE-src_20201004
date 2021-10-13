//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "kernel.h"
#include "excpt.h"

#include "corecrt.h"

#include <ehdata.h>     // Declarations of all types used for EH
#include <trnsctrl.h>   // Routines to handle transfer of control
#include <ehstate.h>    // Declarations of state management stuff
#include <exception>    // User-visible routines for eh






////////////////////////////////////////////////////////////////////////////////
//
// UnHandledExceptionHandler -
//
//

int __cdecl _CxxUnHandledExceptionFilter (EXCEPTION_POINTERS *pPtrs)
{
    if (PER_IS_MSVC_EH((EHExceptionRecord*)(pPtrs->ExceptionRecord))) {
        std::terminate();            // Does not return
        return (EXCEPTION_EXECUTE_HANDLER);
    } else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}


extern "C" int __cdecl UnhandledExceptionFilter(struct _EXCEPTION_POINTERS * pPtrs) {
  return _CxxUnHandledExceptionFilter (pPtrs);

}
