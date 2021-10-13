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
*invarg.c - stub for invalid argument handler
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   defines _invalidarg() and _set_invalid_parameter_handler()
*
*Revision History:
*       05-01-03  SSM   Module created
*       04-10-04  AJS   Builds as C++; unsigned short overload of _invalid_parameter for use by managed code
*       04-11-04  AC    Use EncodePointer to store the user handler
*                       VSW#225573
*       10-29-04  DJT   Added functions for secure pointers for use throughout the CRT
*       11-02-04  AC    Call unhandled exception filter directly to invoke watson
*                       VSW#320460
*       01-09-05  AC    Moved initialization of __pInvalidArgHandler in .CRT$XIC
*                       VSW#422266
*       01-12-05  DJT   Use OS-encoded global function pointers
*       03-24-05  AC    Added encode/decode function which do not take the loader lock
*                       VSW#473724
*       03-23-05  MSL   Review comment cleanup
*       03-31-05  PAL   Don't invoke _CRT_DEBUGGER_HOOK on invalid parameter if the
*                       user has defined a handler.
*       05-06-05  AC    Add _invalid_parameter_noinfo, to have better code generation with Secure SCL
*       05-25-05  AI    Call the debugger hook, inside __invoke_watson, if the exception search filter
*                       found no handler and if there was no debugger previously attached.
*                       VSW#495287
*       05-25-07  ATC   updated how we call invoke watson in AMD64 according to the windows CRT
*
*******************************************************************************/

#include <cruntime.h>
#include <sect_attribs.h>
#include <awint.h>
#include <internal.h>
#include <rterr.h>
#include <stdlib.h>
#include <windows.h>
#include <dbgint.h>
#include <intrin.h>

#ifdef  __cplusplus
extern "C" {
#endif

void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)

#if defined(_M_IX86) || defined (_M_X64)
void * _AddressOfReturnAddress(void);
#pragma intrinsic(_AddressOfReturnAddress)
#endif

/* global variable which stores the user handler */

_invalid_parameter_handler __pInvalidArgHandler;

extern "C"
void _initp_misc_invarg(void* enull)
{
    __pInvalidArgHandler = (_invalid_parameter_handler) enull;
}

/***
*void _invalid_arg() -
*
*Purpose:
*   Called when an invalid argument is passed into a CRT function
*
*Entry:
*   pszExpression - validation expression (can be NULL)
*   pszFunction - function name (can be NULL)
*   pszFile - file name (can be NULL)
*   nLine - line number
*   pReserved - reserved for future use
*
*Exit:
*   return from this function results in error being returned from the function
*   that called the handler
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP void __cdecl _invalid_parameter(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
    _invalid_parameter_handler pHandler = __pInvalidArgHandler;

    pszExpression;
    pszFunction;
    pszFile;

    pHandler = (_invalid_parameter_handler) DecodePointer(pHandler);
    if (pHandler != NULL)
    {
        pHandler(pszExpression, pszFunction, pszFile, nLine, pReserved);
        return;
    }

    _invoke_watson(pszExpression, pszFunction, pszFile, nLine, pReserved);
}

#ifndef _DEBUG

/* wrapper which passes no debug info; not available in debug;
 * we don't pass the null params, so we gain some
 * speed and space in code generation 
 */
_CRTIMP void __cdecl _invalid_parameter_noinfo(void)
{
    _invalid_parameter(NULL, NULL, NULL, 0, 0);
}

/* wrapper which passes no debug info; not available in debug;
 * we don't pass the null params, so we gain some
 * speed and space in code generation; also, we make sure we never return;
 * this is used only in inline code (mainly in the Standard C++ Library and
 * in the SafeInt Library); with __declspec(noreturn) the compiler BE can
 * optimized the codegen even better
 */
_CRTIMP __declspec(noreturn) void __cdecl _invalid_parameter_noinfo_noreturn(void)
{
    _invalid_parameter(NULL, NULL, NULL, 0, 0);
    _invoke_watson(NULL, NULL, NULL, 0, 0);
}

#endif

_CRTIMP __declspec(noreturn) void __cdecl _invoke_watson(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
    (pszExpression);
    (pszFunction);
    (pszFile);
    (nLine);
    (pReserved);

#ifndef _WIN32_WCE // CE doesn't support __fastfail
#if defined(_M_IX86) || defined(_M_X64)
    if (IsProcessorFeaturePresent(PF_FASTFAIL_AVAILABLE))
#endif
        __fastfail(FAST_FAIL_INVALID_ARG);
#else
#ifdef _M_ARM
	// CE doesn't support __fastfail. So we break into
	// debugger to appraise the user of the fault.
	DebugBreak();
	TerminateProcess(GetCurrentProcess(), STATUS_INVALID_PARAMETER);
#endif
#endif /* _WIN32_WCE */

#if defined(_M_IX86) || defined(_M_X64)
    /* Raise a failfast exception and terminate the process */
	_call_reportfault(_CRT_DEBUGGER_INVALIDPARAMETER,
#ifdef _WIN32_WCE
					  STATUS_INVALID_PARAMETER,
#else
                      STATUS_INVALID_CRUNTIME_PARAMETER,
#endif
                       EXCEPTION_NONCONTINUABLE);

    __crtTerminateProcess(STATUS_INVALID_CRUNTIME_PARAMETER);
#endif /* defined(_M_IX86) || defined(_M_X64) */
}

#if defined(_M_IX86) || defined(_M_X64)
void __cdecl _call_reportfault(
    int nDbgHookCode,
    DWORD dwExceptionCode,
    DWORD dwExceptionFlags
    )
{

#ifndef _WIN32_WCE
    // Notify the debugger if attached.
    if (nDbgHookCode != _CRT_DEBUGGER_IGNORE)
        _CRT_DEBUGGER_HOOK(nDbgHookCode);

    /* Fake an exception to call reportfault. */
    EXCEPTION_RECORD   ExceptionRecord = {0};
    CONTEXT ContextRecord;
    EXCEPTION_POINTERS ExceptionPointers = {&ExceptionRecord, &ContextRecord};
    BOOL wasDebuggerPresent = FALSE;
    DWORD ret = 0;

#if defined(_M_IX86)

    __asm {
        mov dword ptr [ContextRecord.Eax], eax
        mov dword ptr [ContextRecord.Ecx], ecx
        mov dword ptr [ContextRecord.Edx], edx
        mov dword ptr [ContextRecord.Ebx], ebx
        mov dword ptr [ContextRecord.Esi], esi
        mov dword ptr [ContextRecord.Edi], edi
        mov word ptr [ContextRecord.SegSs], ss
        mov word ptr [ContextRecord.SegCs], cs
        mov word ptr [ContextRecord.SegDs], ds
        mov word ptr [ContextRecord.SegEs], es
        mov word ptr [ContextRecord.SegFs], fs
        mov word ptr [ContextRecord.SegGs], gs
        pushfd
        pop [ContextRecord.EFlags]
    }

    ContextRecord.ContextFlags = CONTEXT_CONTROL;
#pragma warning(push)
#pragma warning(disable:4311)
    ContextRecord.Eip = (ULONG)_ReturnAddress();
    ContextRecord.Esp = (ULONG)_AddressOfReturnAddress();
#pragma warning(pop)
    ContextRecord.Ebp = *((ULONG *)_AddressOfReturnAddress()-1);

#elif defined(_M_X64)
    __crtCaptureCurrentContext(&ContextRecord);
    ContextRecord.Rip = (ULONGLONG) _ReturnAddress();
    ContextRecord.Rsp = (ULONGLONG) _AddressOfReturnAddress()+8;

#endif

    ExceptionRecord.ExceptionCode = dwExceptionCode;
    ExceptionRecord.ExceptionFlags    = dwExceptionFlags;
    ExceptionRecord.ExceptionAddress = _ReturnAddress();

    wasDebuggerPresent = IsDebuggerPresent();

    /* Raises an exception that bypasses all exception handlers. */
    ret = __crtUnhandledException(&ExceptionPointers);

    // if no handler found and no debugger previously attached
    // the execution must stop into the debugger hook.
    if (ret == EXCEPTION_CONTINUE_SEARCH && !wasDebuggerPresent && nDbgHookCode != _CRT_DEBUGGER_IGNORE) {
        _CRT_DEBUGGER_HOOK(nDbgHookCode); 
    }
#else
	// CE doesn't support the above logic to raise exception
	// bypassing all exception handlers. So we break into
	// debugger to appraise the user of the fault.
	DebugBreak();
#endif // _WIN32_WCE
}
#endif /* defined(_M_IX86) || defined(_M_X64) */

/***
*void _set_invalid_parameter_handler(void) -
*
*Purpose:
*       Establish a handler to be called when a CRT detects a invalid parameter
*
*       This function is not thread-safe
*
*Entry:
*   New handler
*
*Exit:
*   Old handler
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP _invalid_parameter_handler __cdecl
_set_invalid_parameter_handler( _invalid_parameter_handler pNew )
{
    _invalid_parameter_handler pOld = NULL;

    pOld = __pInvalidArgHandler;
    pOld = (_invalid_parameter_handler) DecodePointer((PVOID)pOld);
    pNew = (_invalid_parameter_handler) EncodePointer((PVOID)pNew);

    __pInvalidArgHandler = pNew;

    return pOld;
}

_CRTIMP _invalid_parameter_handler __cdecl
_get_invalid_parameter_handler( )
{
    _invalid_parameter_handler pOld = NULL;

    pOld = __pInvalidArgHandler;
    pOld = (_invalid_parameter_handler) DecodePointer((PVOID)pOld);

    return pOld;
}

#ifdef  __cplusplus
}
#endif

#ifdef  __cplusplus

#if defined(_NATIVE_WCHAR_T_DEFINED)
/*
unsigned short -> wchar_t
This is defined as a real function rather than an alias because the parameter types
are different in the two versions which means different metadata is required when building /clr apps.
*/
extern "C++" _CRTIMP void __cdecl _invalid_parameter(
    const unsigned short * pszExpression,
    const unsigned short * pszFunction,
    const unsigned short * pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
    _invalid_parameter(
        reinterpret_cast<const wchar_t *>(pszExpression),
        reinterpret_cast<const wchar_t *>(pszFunction),
        reinterpret_cast<const wchar_t *>(pszFile),
        nLine,
        pReserved
        );
}

extern "C++" __declspec(noreturn) void __cdecl _invoke_watson(
    const unsigned short * pszExpression,
    const unsigned short * pszFunction,
    const unsigned short * pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
    _invoke_watson(
        reinterpret_cast<const wchar_t *>(pszExpression),
        reinterpret_cast<const wchar_t *>(pszFunction),
        reinterpret_cast<const wchar_t *>(pszFile),
        nLine,
        pReserved
        );
}

#endif // defined(_NATIVE_WCHAR_T_DEFINED)

#endif // __cplusplus
