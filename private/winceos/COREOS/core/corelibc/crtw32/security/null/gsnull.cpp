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
 *    gsnull.c
 *
 * Abstract:
 *
 *  This file contains the most basic implementation of compiler /GS
 *  (enable security checks) switch support.  It resolves runtime
 *  references but has no external dependencies itself.
 *
 *  N.B. USE OF THIS IMPLEMENTATION IS NOT RECOMMENDED
 *
 *  Under ideal conditions, the global security cookie should be
 *  initialized with a random value.  This library should only be
 *  used if no source of randomness is available.
 *
 *--
 */
#include <corecrt.h>

/*
 * The global security cookie.  This name is known to the compiler.
 * Initialize to a garbage non-zero value just in case we have a buffer overrun
 * in any code that gets run before __security_init_cookie() has a chance to
 * initialize the cookie to the final value.
 */
extern "C"
{
__declspec(selectany) UINT_PTR __security_cookie = DEFAULT_SECURITY_COOKIE;
__declspec(selectany) UINT_PTR __security_cookie_complement = ~DEFAULT_SECURITY_COOKIE;
}

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

extern "C"
void
__cdecl
__security_init_cookie(
    void
    )
{
    if (__security_cookie == 0)
    {
        /*
         * The default implementation of __security_check_cookie requires
         * that the most significant 16-bits of the cookie are zero.
         */
        __security_cookie = DEFAULT_SECURITY_COOKIE;
    }
}


/***
*__report_gsfailure() - report a security error.
*
*Purpose:
*       Loop forever
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
__report_gsfailure(
    void
    )
{
    /*
     * This function must not return or throw an exception under any
     * circumstances.
     */

    for (;;)
        ;
}

