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
*fgetc.c - get a character from a stream
*
*
*Purpose:
*   defines fgetc() and getc() - read  a character from a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

/***
*int fgetc(stream), getc(stream) - read a character from a stream
*
*Purpose:
*   reads a character from the given stream
*
*Entry:
*   FILEX *stream - stream to read character from
*
*Exit:
*   returns the character read
*   returns EOF if at end of file or error occurred
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fgetc (
    REG1 FILEX *stream
    )
{
    int retval;

    _VALIDATE_RETURN((stream != NULL), EINVAL, EOF);

    if (!_lock_validate_str(stream))
        return EOF;

    __STREAM_TRY
    {
        retval = _getc_lk(stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}

int __cdecl getchar(void)
{
    /* Initialize before using *internal* stdin */
    if(!CheckStdioInit())
        return EOF;

    return fgetc(stdin);
}

