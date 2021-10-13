//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*secchk.c - checks buffer overrun security cookie
*
*Purpose:
*       Defines compiler helper __security_check_cookie, used by the /GS
*       compile switch to detect local buffer variable overrun bugs/attacks.
*
*       When compiling /GS, the compiler injects code to detect when a local
*       array variable has been overwritten, potentially overwriting the
*       return address (on machines like x86 where the return address is on
*       the stack).  A local variable is allocated directly before the return
*       address and initialized on entering the function.  When exiting the
*       function, the compiler inserts code to verify that the local variable
*       has not been modified.  If it has, then an error reporting routine
*       is called.
*
*******************************************************************************/

#include <corecrt.h>
#include <windows.h>
#include <stdlib.h>

/*
 * The global security cookie.  This name is known to the compiler.
 */
extern DWORD_PTR __security_cookie;

/*
 * Trigger initialization of the global security cookie on program startup.
 * Force initialization before any #pragma init_seg() inits by using .CRT$XCAA
 * as the startup funcptr section.
 */

#pragma data_seg(".CRT$XCAA")
extern void __cdecl __security_init_cookie(void);
static __declspec(allocate(".CRT$XCAA")) _PVFV init_cookie = __security_init_cookie;
#pragma data_seg()

static void __cdecl report_failure(void);

void __cdecl __security_error_handler(int, void *);

/***
*__security_check_cookie(cookie) - check for buffer overrun
*
*Purpose:
*       Compiler helper.  Check if a local copy of the security cookie still
*       matches the global value.  If not, then report the fatal error.
*
*       The actual reporting is split out into static helper report_failure,
*       since the cookie check routine must be minimal code that preserves
*       any registers used in returning the callee's result.
*
*Entry:
*       DWORD_PTR cookie - local security cookie to check
*
*Exit:
*       Returns immediately if the local cookie matches the global version.
*       Otherwise, calls the failure reporting handler and exits.
*
*Exceptions:
*
*******************************************************************************/


#if defined(_M_IX86)

void __declspec(naked) __fastcall __security_check_cookie(DWORD_PTR cookie)
{
    /* x86 version written in asm to preserve all regs */
    __asm {
        cmp ecx, __security_cookie
        jne failure
        ret
failure:
        jmp report_failure
    }
}

#else

void __cdecl __security_check_cookie(DWORD_PTR cookie)
{
    /* Immediately return if the local cookie is OK. */
    if (cookie == __security_cookie)
        return;

    /* Report the failure */
    report_failure();
}

#endif

static void __cdecl report_failure(void)
{
    /* Report the failure */
    __try {
        __security_error_handler(_SECERR_BUFFER_OVERRUN, NULL);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        /* nothing */
    }

    ExitProcess(STATUS_STACK_BUFFER_OVERRUN);
}
