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
*putwch.c - write a wide character to console
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _putwch() - writes a wide character to a console
*
*Revision History:
*       02-11-00  GB    Module Created.
*       04-25-00  GB    Made putwch more robust in using WriteConsoleW
*       05-17-00  GB    Use ERROR_CALL_NOT_IMPLEMENTED for existance of W API
*       11-22-00  PML   Wide-char *putwc* functions take a wchar_t, not wint_t.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-01-03  SJ    VSW#175081 - Incorrect Return Value
*       03-13-04  MSL   Avoid taking array address for prefast
*
*******************************************************************************/

#ifndef _POSIX_

#ifndef _WIN32_WCE

#include <stdlib.h>
#include <conio.h>
#include <io.h>
#include <errno.h>
#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <limits.h>

/*
 * declaration for console handle
 */
extern intptr_t _confh;

/***
*wint_t _putwch(ch) - write a wide character to a console
*
*Purpose:
*       Writes a wide character to a console.
*
*Entry:
*       wchar_t ch - wide character to write
*
*Exit:
*       returns the wide character if successful
*       returns WEOF if fails
*
*Exceptions:
*
*******************************************************************************/

wint_t _CRTIMP __cdecl _putwch (
        wchar_t ch
        )
{
        wint_t retval;

        _mlock(_CONIO_LOCK);
        __try {

        retval = _putwch_nolock(ch);

        }
        __finally {
                _munlock(_CONIO_LOCK);
        }

        return(retval);
}

/***
*_putwch_nolock() -  _putwch() core routine (locked version)
*
*Purpose:
*       Core _putwch() routine; assumes stream is already locked.
*
*       [See _putwch() above for more info.]
*
*Entry: [See _putwch()]
*
*Exit:  [See _putwch()]
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl _putwch_nolock (
        wchar_t ch
        )
{
    DWORD cchWritten;

    if (_confh == -2)
        __initconout();

    if (_confh == -1)
        return WEOF;

    /* write character to console file handle */

    if (!WriteConsoleW((HANDLE) _confh,
                       &ch,
                       1,
                       &cchWritten,
                       NULL))
    {
        return WEOF;
    }

    return ch;
}

#endif // _WIN32_WCE

#endif /* _POSIX_ */
