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

extern BOOL WINAPI _CRTDLL_INIT(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved);
extern BOOL g_fCrtLegacyInputValidation;

void CrtEntry(BOOL fStartup)
{
    if (fStartup)
    {
        // Initialize the compiler /GS security cookie
        // This must happen before any additional threads are created

        __security_init_cookie();

        InitCRTStorage();

        // Applications built with pre-CE6 SDKs should not be terminated
        // due to bad parameters

        g_fCrtLegacyInputValidation = (HIWORD(GetProcessVersion(0)) < 6) ? TRUE : FALSE;

        _CRTDLL_INIT(NULL, DLL_PROCESS_ATTACH, 0);
    }
    else
    {
        _CRTDLL_INIT(NULL, DLL_PROCESS_DETACH, 0);
    }
}


__declspec(noreturn)
void
__cdecl
__crt_fatal(
    CONTEXT * pContextRecord,
    DWORD ExceptionCode
    )
/*++

Description:

    Create a fake exception record using the given context and report a fatal
    error via Windows Error Reporting.  Then either end the current process
    or, if in kernel mode, reboot the device.

Arguments:

    pContextRecord - Supplies a pointer to the context reflecting CPU state at
        the time of the error.

    ExceptionCode - Supplies the exception code reported and the argument to
        ExitProcess in user mode.

--*/
{

    EXCEPTION_RECORD ExceptionRecord;
    EXCEPTION_POINTERS ExceptionPointers;

    ZeroMemory(&ExceptionRecord, sizeof(ExceptionRecord));

    ExceptionRecord.ExceptionCode = ExceptionCode;
    ExceptionRecord.ExceptionAddress = (PVOID)CONTEXT_TO_PROGRAM_COUNTER(pContextRecord);

    ExceptionPointers.ExceptionRecord = &ExceptionRecord;
    ExceptionPointers.ContextRecord = pContextRecord;

    ReportFault(&ExceptionPointers, 0);

    // Notify the debugger if connected.

    DebugBreak();

#if defined(KCOREDLL)

    // Give the OAL a chance to restart or shutdown

    KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
    KernelIoControl(IOCTL_HAL_HALT, NULL, 0, NULL, 0, NULL);

#else

    // Terminate the current process as soon as possible

    ExitProcess(ExceptionCode);

#endif

    // Do not return to the caller under any circumstances

    for (;;)
    {
        Sleep(INFINITE);
    }
}


void
__cdecl
__crt_unrecoverable_error(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
/*++

Description:

    Log the fact that an unrecoverable error occurred

--*/
{
    // Parameters are ignored but may be useful when debugging.

    (pszExpression);
    (pszFunction);
    (pszFile);
    (nLine);
    (pReserved);

    if (g_fCrtLegacyInputValidation)
    {
        // Be more lenient to applications built
        // for older OS versions.

        return;
    }

    // Capture the context for error reporting.

    CONTEXT ContextRecord;

    _CRT_CAPTURE_CONTEXT(&ContextRecord);

    __crt_fatal(&ContextRecord, (DWORD)STATUS_INVALID_PARAMETER);
}


DWORD *
__crt_get_storage_flags()
/*++

Description:

    Return a pointer to a the CRT flags.

--*/
{
    return &GetCRTStorage()->dwFlags;
}


DWORD *
__crt_get_kernel_flags()
/*++

Description:

    Return a pointer to the kernel's user-mode TLS slot.

--*/
{
    return &UTlsPtr()[TLSSLOT_KERNEL];
}


char *
__crt_get_storage__fcvt()
/*++

Description:

    Return a pointer to a buffer of size _CVTBUFSIZE for use by _fcvt and
    _ecvt.

--*/
{
    crtGlob_t * pcrtg;

    pcrtg = GetCRTStorage();
    if (pcrtg->pchfpcvtbuf == NULL)
    {
        pcrtg->pchfpcvtbuf = (char *)malloc(_CVTBUFSIZE);
    }

    return pcrtg->pchfpcvtbuf;
}


long *
__crt_get_storage_rand()
/*++

Description:

    Return a pointer to a long for use by rand and srand.

--*/
{
    return &GetCRTStorage()->rand;
}


char * *
__crt_get_storage_strtok()
/*++

Description:

    Return a pointer to a char* for use by strtok.

--*/
{
    return &GetCRTStorage()->nexttoken;
}


wchar_t * *
__crt_get_storage_wcstok()
/*++

Description:

    Return a pointer to a wchar_t* for use by wcstok.

--*/
{
    return &GetCRTStorage()->wnexttoken;
}


crtEHData *
__crt_get_storage_EH()
/*++

Description:

    Return a pointer to a wchar_t* for use by wcstok.

--*/
{
    return &GetCRTStorage()->EHData;
}


/*
 * Include implementations from buildflb
 */
#include <exitproc.c>
#include <raiseexc.c>


BOOL
__crtIsDebuggerPresent(void)
{
    return IsDebuggerPresent();
}


DWORD
GetCRTFlags(
    void
    )
{
    return *__crt_get_storage_flags();
}


void
SetCRTFlag(
    DWORD dwFlag
    )
{
    *__crt_get_storage_flags() |= dwFlag;
}


void
ClearCRTFlag(
    DWORD dwFlag
    )
{
    *__crt_get_storage_flags() &= ~dwFlag;
}


void
InitCRTStorage(
    void
    )
{
    crtGlob_t* pcrtg = GetCRTStorage();
    memset(pcrtg, 0, sizeof(*pcrtg));
    pcrtg->rand = 1;
}


void
ClearCRTStorage()
{
    crtGlob_t* pcrtg = GetCRTStorage();

    // Crt storage can be trashed due to user error. Try-except here such that
    // thread/process can exit normally.

    __try
    {
        if (pcrtg->pchfpcvtbuf)
        {
            LocalFree((HLOCAL)pcrtg->pchfpcvtbuf);
        }
        memset(pcrtg, 0, sizeof(*pcrtg));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

} // extern "C"

