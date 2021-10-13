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
*Purpose:
*       defines gets() and getws() - read a line from stdin into buffer
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <mtdll.h>
#include <crttchar.h>
#include <limits.h>

#ifdef CRT_UNICODE
#define _getchar_helper _getwchar_lk
#else
#define _getchar_helper _getchar_lk
#endif

#ifdef CRT_UNICODE
#define _getts_helper _getws_helper
#else
#define _getts_helper _gets_helper
#endif

static CRT__TCHAR * __cdecl _getts_helper (
    CRT__TCHAR *string,
    size_t bufferSize,
    int earlyOutIfEOFIsFirstChar
)
{
    int ch = 0;
    CRT__TCHAR *pointer = string;
    CRT__TCHAR *retval = string;

    _VALIDATE_RETURN((string != NULL), EINVAL, NULL);
    _VALIDATE_RETURN((bufferSize > 0), EINVAL, NULL);

    /* The C Standard states the input buffer should remain
    unchanged if EOF is encountered immediately. Hence we
    do not blank out the input buffer here */

    /* Initialize before using *internal* stdout */
    if(!CheckStdioInit())
        return 0;

    if (!_lock_validate_str(stdin))
        return NULL;

    __STREAM_TRY
    {
#ifndef CRT_UNICODE
// Don't support this
/*
        _VALIDATE_STREAM_ANSI_SETRET(stdin, EINVAL, retval, NULL);
        if (retval == NULL)
        {
            goto done;
        }
*/
#endif

        /* special case: check if the first char is EOF and treat it differently if the user requested so */
        ch = _getchar_helper();
        if (ch == CRT_TEOF)
        {
            retval = NULL;
            if (earlyOutIfEOFIsFirstChar)
                goto done;
        }

        if (bufferSize == SIZE_MAX)
        {
            /* insecure case: no buffer size check, no debug filling */
            while (ch != CRT_T('\n') && ch != CRT_TEOF)
            {
                *pointer++ = (CRT__TCHAR)ch;
                ch = _getchar_helper();
            }
            *pointer = 0;
        }
        else
        {
            /* secure case, check buffer size; if buffer overflow, keep on reading until /n or EOF */
            size_t available = bufferSize;
            while (ch != CRT_T('\n') && ch != CRT_TEOF)
            {
                if (available > 0)
                {
                    --available;
                    *pointer++ = (CRT__TCHAR)ch;
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
    __STREAM_FINALLY
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

CRT__TCHAR * __cdecl _getts_s (
    CRT__TCHAR *string,
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

CRT__TCHAR * __cdecl _getts (
    CRT__TCHAR *string
)
{
    return _getts_helper(string, SIZE_MAX, 1 /* early out if EOF is first char read */);
}

