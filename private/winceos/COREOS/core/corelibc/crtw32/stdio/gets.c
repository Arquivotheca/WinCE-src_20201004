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
*gets.c - read a line from stdin
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines gets() and getws() - read a line from stdin into buffer
*
*Revision History:
*       09-02-83  RN    initial version
*       11-06-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-27-88  PHG   Merged DLL and normal versions
*       02-15-90  GJF   Fixed copyright, indents
*       03-19-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-24-90  SBM   Replaced <assertm.h> by <assert.h>
*       08-14-90  SBM   Compiles cleanly with -W3
*       10-02-90  GJF   New-style function declarator.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       01-31-94  CFW   Unicode enable.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       02-22-95  GJF   Replaced WPRFLAG with _UNICODE.
*       03-07-95  GJF   Use _[un]lock_str2 instead of _[un]lock_str. Also,
*                       removed useless local and macros.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-01-03  SJ    VSW#175924 - gets_s to match gets in return value
*       12-02-03  SJ    Reroute Unicode I/O
*       03-13-04  MSL   Avoid returning from __try for prefast
*       11-11-04  AC    Using (size_t)-1 instead of UINT_MAX
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <mtdll.h>
#include <tchar.h>
#include <limits.h>

#ifdef _UNICODE
#define _getchar_helper _getwchar_nolock
#else
#define _getchar_helper _getchar_nolock
#endif

_TCHAR * __cdecl _getts_helper (
    _TCHAR *string,
    size_t bufferSize,
    int earlyOutIfEOFIsFirstChar
)
{
    int ch = 0;
    _TCHAR *pointer = string;
    _TCHAR *retval = string;
    
    _VALIDATE_RETURN( (string != NULL), EINVAL, NULL);
    _VALIDATE_RETURN( (bufferSize > 0), EINVAL, NULL);
    _CHECK_IO_INIT(NULL);
    
    /* The C Standard states the input buffer should remain
    unchanged if EOF is encountered immediately. Hence we 
    do not blank out the input buffer here */

    _lock_str2(0, stdin);
    __try {
#ifndef _UNICODE
        _VALIDATE_STREAM_ANSI_SETRET(stdin, EINVAL, retval, NULL);
        if (retval == NULL)
        {
            goto done;
        }
#endif

        /* special case: check if the first char is EOF and treat it differently if the user requested so */
        ch = _getchar_helper();
        if (ch == _TEOF)
        {
            retval = NULL;
            if (earlyOutIfEOFIsFirstChar)
                goto done;
        }

        if (bufferSize == (size_t)-1)
        {
            /* insecure case: no buffer size check, no debug filling */
            while (ch != _T('\n') && ch != _TEOF)
            {
                *pointer++ = (_TCHAR)ch;
                ch = _getchar_helper();
            }
            *pointer = 0;
        }
        else
        {
            /* secure case, check buffer size; if buffer overflow, keep on reading until /n or EOF */
            size_t available = bufferSize;
            while (ch != _T('\n') && ch != _TEOF)
            {
                if (available > 0)
                {
                    --available;
                    *pointer++ = (_TCHAR)ch;
                }
                ch = _getchar_helper();
            }
            if (available == 0)
            {
                _RESET_STRING(string, bufferSize);
                _RETURN_BUFFER_TOO_SMALL_ERROR(string, bufferSize, NULL);
            }
            *pointer = 0;
            _FILL_STRING(string, bufferSize, bufferSize - available + 1);
        }

        /* Common return */
done: ;

    }
    __finally
    {
        _unlock_str2(0, stdin);
    }

    return retval;
}

/***
*errno_t gets_s(string, sz) - read a line from stdin
*
*Purpose:
*       
*
*Entry:
*       char *string      - place to store read string.
*       size_t bufferSize - length of dest buffer
*
*Exit:
*       returns string, filled in with the line of input
*       null string if \n found immediately or if EOF found immediately
*Exceptions:
*
*******************************************************************************/

_TCHAR * __cdecl _getts_s (
    _TCHAR *string,
    size_t bufferSize
)
{
    return _getts_helper(string, bufferSize, 0 /* treat first EOF as \n */);
}

/***
*char *gets(string) - read a line from stdin
*
*Purpose:
*       Gets a string from stdin terminated by '\n' or EOF; don't include '\n';
*       append '\0'.
*
*Entry:
*       char *string - place to store read string, assumes enough room.
*
*Exit:
*       returns string, filled in with the line of input
*       null string if \n found immediately
*       NULL if EOF found immediately
*
*Exceptions:
*
*******************************************************************************/

_TCHAR * __cdecl _getts (
    _TCHAR *string
)
{
    return _getts_helper(string, (size_t)-1, 1 /* early out if EOF is first char read */);
}
