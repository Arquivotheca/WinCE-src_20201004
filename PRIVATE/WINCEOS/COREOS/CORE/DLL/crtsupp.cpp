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

Module:

    crtsupp.cpp

Description:

    Provide COREDLL-specific implementations of CRT support routines

--*/

#include <corecrt.h>
#include <corecrtstorage.h>
#include <errorrep.h>
#include <stdlib.h>


extern "C"
{

void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)

#ifdef _X86_
void * _AddressOfReturnAddress(void);
#pragma intrinsic(_AddressOfReturnAddress)
#endif


void
__cdecl
__crt_unrecoverable_error(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
    {
    /* Fake an exception to call reportfault. */
    EXCEPTION_RECORD ExceptionRecord;
    CONTEXT ContextRecord;
    EXCEPTION_POINTERS ExceptionPointers;

    (pszExpression);
    (pszFunction);
    (pszFile);
    (nLine);
    (pReserved);

#if defined(_X86_)

    __asm
        {
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

#elif defined(_ARM_) || defined(_MIPS_) || defined(_SHX_)

    _CRT_CAPTURE_CONTEXT(&ContextRecord);

#else

    ZeroMemory(&ContextRecord, sizeof(ContextRecord));

#endif

    ZeroMemory(&ExceptionRecord, sizeof(ExceptionRecord));

    ExceptionRecord.ExceptionCode = STATUS_INVALID_PARAMETER;
    ExceptionRecord.ExceptionAddress = _ReturnAddress();

    ExceptionPointers.ExceptionRecord = &ExceptionRecord;
    ExceptionPointers.ContextRecord = &ContextRecord;

    ReportFault(&ExceptionPointers, 0);

    if (IsDebuggerPresent()) DebugBreak();

    ExitProcess(STATUS_INVALID_PARAMETER);
}

char*
__crt_get_storage__fcvt()
/*++

Description:
    Return a pointer to a buffer of size _CVTBUFSIZE for use by _fcvt and
    _ecvt

--*/
    {
    crtGlob_t * pcrtg;

    pcrtg = GetCRTStorage();
    if (!pcrtg)
        {
        return NULL;
        }

    if (pcrtg->pchfpcvtbuf == NULL)
        {
        pcrtg->pchfpcvtbuf = (char *)malloc(_CVTBUFSIZE);
        if (pcrtg->pchfpcvtbuf == NULL)
            {
            return NULL;
            }
        }

    return pcrtg->pchfpcvtbuf;
    }

long*
__crt_get_storage_rand()
/*++

Description:
    Return a pointer to a long for use by rand and srand

--*/
    {
    crtGlob_t * pcrtg;
    pcrtg = GetCRTStorage();
    if (!pcrtg)
        {
        return NULL;
        }

    return &pcrtg->rand;
    }

char**
__crt_get_storage_strtok()
/*++

Description:
    Return a pointer to a char* for use by strtok

--*/
    {
    crtGlob_t * pcrtg;
    pcrtg = GetCRTStorage();
    if (!pcrtg)
        {
        return NULL;
        }

    return &pcrtg->nexttoken;
    }

wchar_t**
__crt_get_storage_wcstok()
/*++

Description:
    Return a pointer to a wchar_t* for use by wcstok

--*/
    {
    crtGlob_t * pcrtg;
    pcrtg = GetCRTStorage();
    if (!pcrtg)
        {
        return NULL;
        }

    return &pcrtg->wnexttoken;
    }

crtEHData*
__crt_get_storage_EH()
/*++

Description:
    Return a pointer to a wchar_t* for use by wcstok

--*/
    {
    crtGlob_t * pcrtg;
    pcrtg = GetCRTStorage();
    if (!pcrtg)
        {
        return NULL;
        }

    return &pcrtg->EHData;
    }

BOOL
__crtExitProcess(
    DWORD uExitCode
    )
{
    return ExitProcess(uExitCode);
}

VOID
__crtRaiseException(
    DWORD         dwExceptionCode,
    DWORD         dwExceptionFlags,
    DWORD         nNumberOfArguments,
    CONST DWORD * lpArguments
    )
{
    RaiseException(dwExceptionCode, dwExceptionFlags, nNumberOfArguments, lpArguments);
}

} // extern "C"

