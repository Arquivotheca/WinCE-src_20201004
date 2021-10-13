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

//
// __crt_fatal should be declared __declspec(noreturn), but this causes the ARM
// compiler to simply drop all subsequent code after the call (BL).  This
// results in __crt_fatal's return address pointing outside of
// __report_gsfailure_context, which breaks the stack unwind.  This is either
// a compiler bug (doesn't prevent BL being the last instruction in a region)
// or a debugger bug (doesn't back up the return address like the OS unwinder
// does).  Normally, this bug isn't a big deal, but we work around here
// because it is critical that this function's stack frame will be unwindable
// since it will appear in many Watson dumps.
//
// The workaround of not declaring __crt_fatal as "noreturn" causes the
// compiler to favor optimizing this as a tail call, removing
// __report_gsfailure_context's frame from the callstack altogether.
//

extern "C"
//__declspec(noreturn)
void
__cdecl
__crt_fatal(
    CONTEXT * pContext,
    DWORD ExceptionCode
    );

/***
*__security_gen_cookie2() - create buffer overrun security cookie.
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
UINT_PTR
__cdecl
__security_gen_cookie2()
{
    UINT_PTR cookie = 0;
    FILETIME ft = {0};
    LARGE_INTEGER perfctr = {0};

    /*
     * Initialize the global cookie with an unpredictable value which is
     * different for each module in a process.  Combine a number of sources
     * of randomness.
     */

    GetCurrentFT(&ft);
    cookie ^= ft.dwLowDateTime;
    cookie ^= ft.dwHighDateTime;

    cookie ^= GetCurrentProcessId();
    cookie ^= GetCurrentThreadId();
    cookie ^= GetTickCount();

    QueryPerformanceCounter(&perfctr);
    cookie ^= perfctr.LowPart;
    cookie ^= perfctr.HighPart;

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
UINT_PTR
__cdecl
__security_gen_cookie()
{
    UINT_PTR cookie = 0;
    UINT_PTR tmp;

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
    tmp = cookie & (((UINT_PTR)-1) >> 16);
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
__declspec(noreturn)
void
__cdecl
__report_gsfailure_context(
    CONTEXT * pContextRecord
    )
{
    __crt_fatal(pContextRecord, (DWORD)STATUS_STACK_BUFFER_OVERRUN);
}
