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
*fputws.c - write a string to a stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fputws() - writes a string to a stream
*
*Revision History:
*       04-26-93  CFW   Module created.
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       02-27-98  GJF   Exception-safe locking.
*       01-04-99  GJF   Changes for 64-bit size_t.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*
*******************************************************************************/

#ifndef _POSIX_

#include <file2.h>
#include <internal.h>
#include <stdio.h>
#include <mtdll.h>
#include <tchar.h>
#include <wchar.h>
#include <dbgint.h>

/***
*int fputws(string, stream) - write a string to a file
*
*Purpose:
*       Output the given string to the stream, don't write the L'\0' or
*       supply a L'\n'. Uses _stbuf and _ftbuf for efficiency reasons.
*
*Entry:
*       wchar_t *string - string to write
*       FILE *stream - stream to write to.
*
*Exit:
*       Good return   = 0
*       Error return  = WEOF
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fputws (
        const wchar_t *string,
        FILE *stream
        )
{
    size_t length;
    int retval = 0;

    _VALIDATE_RETURN((string != NULL), EINVAL, WEOF);
    _VALIDATE_RETURN((stream != NULL), EINVAL, WEOF);

    length = wcslen(string);

    _lock_str(stream);
    __try {
        while (length--)
        {
            if (_putwc_nolock(*string++, stream) == WEOF)
            {
                retval = -1;
                break;
            }
        }
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}

#endif  /* _POSIX_ */
