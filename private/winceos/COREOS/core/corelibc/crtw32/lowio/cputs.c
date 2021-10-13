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
*cputs.c - direct console output
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _cputs() - write string directly to console
*
*Revision History:
*       06-09-89  PHG   Module created, based on asm version
*       03-12-90  GJF   Made calling type _CALLTYPE2 (for now), added #include
*                       <cruntime.h> and fixed the copyright. Also, cleaned up
*                       the formatting a bit.
*       04-10-90  GJF   Now _CALLTYPE1.
*       06-05-90  SBM   Recoded as pure 32-bit, using new file handle state bits
*       07-24-90  SBM   Removed '32' from API names
*       09-28-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       01-16-91  GJF   ANSI naming.
*       02-19-91  SRW   Adapt to OpenFile/CreateFile changes (_WIN32_)
*       02-25-91  MHL   Adapt to ReadFile/WriteFile changes (_WIN32_)
*       07-26-91  GJF   Took out init. stuff and cleaned up the error
*                       handling [_WIN32_].
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-19-93  GJF   Use WriteConsole instead of WriteFile.
*       09-06-94  CFW   Remove Cruiser support.
*       12-08-95  SKS   _confh is now initialized on demand
*       02-07-98  GJF   Changes for Win64: _confh is now an intptr_t.
*       12-18-98  GJF   Changes for 64-bit size_t.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-01-03  SJ    VSW#175081 - Incorrect Return Value
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#include <mtdll.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * declaration for console handle
 */
extern intptr_t _confh;

/***
*int _cputs(string) - put a string to the console
*
*Purpose:
*       Writes the string directly to the console.  No newline
*       is appended.
*
*Entry:
*       char *string - string to write
*
*Exit:
*       Good return = 0
*       Error return = !0
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _cputs (
        const char *string
        )
{
#ifdef _WIN32_WCE
        return ERROR_NOT_SUPPORTED; 
#else
        int error = 0;                   /* error occurred? */

        _VALIDATE_CLEAR_OSSERR_RETURN((string != NULL), EINVAL, -1);

        _mlock(_CONIO_LOCK);             /* acquire console lock */
        __try {
            /* write string to console file handle */

            /*
             * What is more important when writing to console. I don't think if
             * speed should matter too much. This justification is used for writing
             * the string to Console. Here we are converting each and every
             * character to wide character and then writing it to console.
             */
            while(*string)
            {
                if (_putch_nolock(*string++) == EOF) {
                    error = -1;
                    break;
                }
            }
        }
        __finally {
            _munlock(_CONIO_LOCK);          /* release console lock */
        }

        return error;
#endif // _WIN32_WCE
}
