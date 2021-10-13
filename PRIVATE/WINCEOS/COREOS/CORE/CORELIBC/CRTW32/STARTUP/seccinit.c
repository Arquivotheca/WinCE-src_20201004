//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*seccinit.c - initialize the global buffer overrun security cookie
*
*Purpose:
*       Define __security_init_cookie, which is called at startup to initialize
*       the global buffer overrun security cookie used by the /GS compile flag.
*
*******************************************************************************/

#include <corecrt.h>
#include <windows.h>

/*
 * The global security cookie.  This name is known to the compiler.
 */
extern DWORD_PTR __security_cookie;

/*
 * A (possibly) non-static helper routine to save space in each module
 */
DWORD_PTR __security_gen_cookie(void);

/***
*__security_init_cookie() - init buffer overrun security cookie.
*
*Purpose:
*       Initialize the global buffer overrun security cookie which is used by
*       the /GS compile switch to detect overwrites to local array variables
*       the potentially corrupt the return address.  This routine is called
*       at EXE/DLL startup.
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl __security_init_cookie(void)
{
    DWORD_PTR cookie = __security_cookie;

    /*
     * Do nothing if the global cookie has already been initialized.
     */

    if (cookie != 0 && cookie != DEFAULT_SECURITY_COOKIE)
        return;

    cookie = __security_gen_cookie();

    /*
     * Make sure the global cookie is never initialized to zero, since in that
     * case an overrun which sets the local cookie and return address to the
     * same value would go undetected.
     */

    __security_cookie = cookie ? cookie : DEFAULT_SECURITY_COOKIE;
}
