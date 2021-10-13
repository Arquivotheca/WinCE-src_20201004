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
*fputc.c - write a character to an output stream
*
*
*Purpose:
*   defines fputc() - writes a character to a stream
*   defines fputwc() - writes a wide character to a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

/***
*int fputc(ch, stream) - write a character to a stream
*
*Purpose:
*   Writes a character to a stream.  Function version of putc().
*
*Entry:
*   int ch - character to write
*   FILEX *stream - stream to write to
*
*Exit:
*   returns the character if successful
*   returns EOF if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fputc (
    int ch,
    FILEX *str
    )
{
    REG1 FILEX *stream;
    REG2 int retval;

    _VALIDATE_RETURN((str != NULL), EINVAL, EOF);

    /* Init stream pointer */
    stream = str;

    if (!_lock_validate_str(stream))
        return EOF;

    __STREAM_TRY
    {
        retval = _putc_lk(ch,stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}

int __cdecl putchar(int ch)
{
    /* Initialize before using *internal* stdout */
    if(!CheckStdioInit())
        return EOF;

    return(fputc(ch, stdout));
}

