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
*throw.cpp - Implementation of the 'throw' command.
*
*
*Purpose:
*       Implementation of the exception handling 'throw' command.
*
*       Entry points:
*       * __CxxThrowException - does the throw.
*
****/

#include <windows.h>
#include <ehdata.h>
#include <coredll.h>   // define debug zone information
#include <cruntime.h>

#pragma hdrstop
/////////////////////////////////////////////////////////////////////////////
//
// __CxxThrowException - implementation of 'throw'
//
// Description:
//      Builds the NT Exception record, and calls the NT runtime to initiate
//      exception processing.
//
//      Why is pThrowInfo defined as _ThrowInfo?  Because _ThrowInfo is secretly
//      snuck into the compiler, as is the prototype for __CxxThrowException, so
//      we have to use the same type to keep the compiler happy.
//
//      Another result of this is that _CRTIMP can't be used here.  Instead, we
//      synthesisze the -export directive below.
//
// Returns:
//      NEVER.  (until we implement resumable exceptions, that is)
//

/* The compiler expects and demands a particular calling convention
 *  for these helper functions.  The x86 compiler demands __stdcall.
 *  Windows CE RISC compilers demand __cdecl, which is the CE standard.
 */
#undef __stdcall                            // #define'd in windef.h for CE

#ifdef _M_IX86
#define HELPERFNAPI __stdcall
#else
#define HELPERFNAPI __cdecl
#endif

extern "C" void HELPERFNAPI
#ifdef _M_IX86
_CxxThrowException(
#else
__CxxThrowException(
#endif // _M_IX86
    void*           pExceptionObject,   // The object thrown
    _ThrowInfo*     pThrowInfo          // Everything we need to know about it
)
{
    static const EHExceptionRecord ExceptionTemplate = { // A generic exception record
        EH_EXCEPTION_NUMBER,            // Exception number
        EXCEPTION_NONCONTINUABLE,       // Exception flags (we don't do resume)
        NULL,                           // Additional record (none)
        NULL,                           // Address of exception (OS fills in)
        EH_EXCEPTION_PARAMETERS,        // Number of parameters
        {   EH_MAGIC_NUMBER1,           // Our version control magic number
            NULL,                       // pExceptionObject
            NULL }                      // pThrowInfo
    };
    EHExceptionRecord ThisException = ExceptionTemplate;    // This exception

    DEBUGMSG(DBGEH,(TEXT("_CxxThrowExcepton: _pExceptionObject=%08x _pThrowInfo=%08x\r\n"),
                    pExceptionObject, pThrowInfo));

    //
    // Fill in the blanks:
    //

    // ThisException.params.pExceptionObject = pExceptionObject;
    // ThisException.params.pThrowInfo = (ThrowInfo*)pThrowInfo;
    PER_PEXCEPTOBJ(&ThisException) = pExceptionObject;
    PER_PTHROW(&ThisException) = (ThrowInfo*)pThrowInfo;

    //
    // Hand it off to the OS:
    //

    __crtRaiseException(ThisException.ExceptionCode,
                        ThisException.ExceptionFlags,
                        ThisException.NumberParameters,
                        (LPDWORD)&ThisException.params);
}
