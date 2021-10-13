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

/***
*gssupport.c - coredll-specific security error handling routines
*
*Purpose:
*       Define __security_gen_cookie2 and __report_gsfailure_context, which
*       are routines required to support the compiler's runtime security error
*       checks, enabled with the /GS flag.  These routines are coredll-specific.
*       Modules linking with fulllibc will need to implement both of these.
*
*       The compiler helper __security_check_cookie will call
*       __report_gsfailure, which will capture the context and call
*       __report_gsfailure_context.
*
*Entrypoints:
*       __security_gen_cookie2
*       __report_gsfailure_context
*
*******************************************************************************/

#include <windows.h>
#include <errorrep.h>
#if defined(KCOREDLL)
#include <kernel.h>
#else
#include <corecrt.h>
#endif

/***
*__security_gen_cookie2() - init buffer overrun security cookie.
*
*Purpose:
*       Generate a value for the global buffer overrun security cookie, which
*       is used by the /GS compiler switch to detect overwrites to local array
*       variables that potentially corrupt the return address.  This routine is
*       called at EXE/DLL startup by the static __security_init_cookie.
*
*Entry:
*
*Exit:
*       returns the generated value
*
*Exceptions:
*
*******************************************************************************/

extern "C"
DWORD_PTR
__cdecl
__security_gen_cookie2(
    void
    )
{
    DWORD_PTR cookie = 0;
    FILETIME ft;
    LARGE_INTEGER li;

    /*
     * Initialize the global cookie with an unpredictable value which is
     * different for each module in a process.  Combine a number of sources
     * of randomness.
     */

    li.QuadPart = CeGetRandomSeed();
    cookie ^= li.LowPart;
    cookie ^= li.HighPart;

    GetCurrentFT(&ft);
    cookie ^= ft.dwLowDateTime;
    cookie ^= ft.dwHighDateTime;

    cookie ^= GetCurrentProcessId();
    cookie ^= GetCurrentThreadId();

    return cookie;
}


/***
*__security_gen_cookie() - init buffer overrun security cookie.
*
*Purpose:
*       Generate a value for the global buffer overrun security cookie, which
*       is used by the /GS compiler switch to detect overwrites to local array
*       variables that potentially corrupt the return address.  This routine is
*       called at EXE/DLL startup old applications.  This routine insures that
*       the upper 16-bits of the cookie are zero.
*
*Entry:
*
*Exit:
*       returns the generated value
*
*Exceptions:
*
*******************************************************************************/

extern "C"
DWORD_PTR
__cdecl
__security_gen_cookie(
    void
    )
{
    DWORD_PTR cookie = 0;
    DWORD_PTR tmp;

    /*
     * Initialize the global cookie with an unpredictable value which is
     * different for each module in a process.  Combine a number of sources
     * of randomness.
     */
    __security_gen_cookie2();

    /*
     * Zero the upper 16-bits of the cookie to help mitigate an attack
     * that overwrites it by MBS or wchar_t string functions.  This
     * is enforced by __security_check_cookie.
     */
    tmp = cookie & (((DWORD_PTR)-1) >> 16);
    cookie = (cookie >> 16) ^ tmp;

    return cookie;
}


/***
*__report_gsfailure_context() - finish reporting a security error.
*
*Purpose:
*       (USER MODE) Report a security error to Dr. Watson and terminate
*       the current process.
*
*       (KERNEL MODE) Give the system a chance to reboot or halt before
*       locking up completely.
*
*Entry:
*       ContextRecord - machine state captured by __report_gsfailure
*
*Exit:
*       Does not return
*
*Exceptions:
*
*******************************************************************************/

extern "C"
void
__declspec(noreturn)
__cdecl
__report_gsfailure_context(
    PCONTEXT ContextRecord
    )
{
    EXCEPTION_RECORD ExceptionRecord = {0};
    EXCEPTION_POINTERS ExceptionPointers;
    DWORD ExceptionAddress = CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);

    ExceptionRecord.ExceptionCode     = STATUS_STACK_BUFFER_OVERRUN;
    ExceptionRecord.ExceptionAddress  = (PVOID)ExceptionAddress;

    ExceptionPointers.ExceptionRecord = &ExceptionRecord;
    ExceptionPointers.ContextRecord   = ContextRecord;

    /*
     * Report the error via Windows Error Reporting
     */
    ReportFault(&ExceptionPointers, 0);

#if defined(KCOREDLL)
    /*
     * Give the OAL a chance to restart or shutdown
     */
    KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
    KernelIoControl(IOCTL_HAL_HALT, NULL, 0, NULL, 0, NULL);
#else
    /*
     * Terminate the current process as soon as possible
     */
    ExitProcess(STATUS_STACK_BUFFER_OVERRUN);
#endif

    /*
     * Do not return to the caller under any circumstances
     */
    while (TRUE)
    {
        Sleep(INFINITE);
    }
}

