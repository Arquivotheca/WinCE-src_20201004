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
/**     TITLE("Compiler /GS switch support")
 *++
 *
 *
 * Module Name:
 *
 *    gsnk.c
 *
 * Abstract:
 *
 *  This file contains NK-specific support for the compiler's /GS
 *  (enable security checks) switch.
 *
 *--
 */
#include <kernel.h>
#include <crt_ordinals.h>
#include <nkintr.h>
#include <oemglobal.h>

/*
 * The global security cookie.  This name is known to the compiler.
 * Initialize to a garbage non-zero value just in case we have a buffer overrun
 * in any code that gets run before __security_init_cookie() has a chance to
 * initialize the cookie to the final value.
 */
__declspec(selectany) DWORD_PTR __security_cookie = DEFAULT_SECURITY_COOKIE;
__declspec(selectany) DWORD_PTR __security_cookie_complement = ~DEFAULT_SECURITY_COOKIE;


/*
 * Handle to K.COREDLL
 */
extern HMODULE hKCoreDll;

/***
*__security_init_cookie() - init buffer overrun security cookie.
*
*Purpose:
*       Initialize the global buffer overrun security cookie which is used by
*       the /GS compile switch to detect overwrites to local array variables
*       the potentially corrupt the return address.  This routine is called
*       at startup.
*
*******************************************************************************/

void
__cdecl
__security_init_cookie(
    void
    )
{
    typedef DWORD_PTR (* PDWPFNV)(void);

    DWORD_PTR cookie = __security_cookie;
    PDWPFNV pfn___security_gen_cookie2;
    LPCSTR ordinal = (LPCSTR)_ORDINAL___security_gen_cookie2;

    /*
     * If this is hit, there are a few possibilities, including:
     * 1) Globals have not been relocated,
     * 2) Globals have not been initialized, or
     * 3) Something (possibly this function) has already initialized the cookie.
     */
    DEBUGCHK(cookie == DEFAULT_SECURITY_COOKIE);
    DEBUGCHK(hKCoreDll != NULL);
    
    pfn___security_gen_cookie2 = (PDWPFNV)GetProcAddressA(hKCoreDll, ordinal);
    DEBUGCHK(pfn___security_gen_cookie2 != NULL);
    
    cookie = (*pfn___security_gen_cookie2)();

    /*
     * Make sure the global cookie is never initialized to zero, since in that
     * case an overrun which sets the local cookie and return address to the
     * same value would go undetected.
     */
    if (cookie == 0) {
        cookie = DEFAULT_SECURITY_COOKIE;
    }
    __security_cookie = cookie;
    __security_cookie_complement = ~cookie;

    /*
     * Copy kernel's cookie to nk/oal
     */
    *g_pOemGlobal->p__security_cookie = cookie;
    *g_pOemGlobal->p__security_cookie_complement = ~cookie;
}


/***
*__report_gsfailure() - report a security error.
*
*Purpose:
*       Report the security error and reboot or halt the system
*
*Exit:
*       Does not return
*
*Exceptions:
*
*******************************************************************************/

void
__declspec(noreturn)
__cdecl
__report_gsfailure(
    void
    )
{
    DebugBreak();

    /*
     * Restart the system
     */
    OEMIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);

    /*
     * Do not return to the caller under any circumstances
     */
    OEMIoControl(IOCTL_HAL_HALT, NULL, 0, NULL, 0, NULL);
    INTERRUPTS_OFF();
    for ( ; ; )
    {
        __noop();
    }
}

