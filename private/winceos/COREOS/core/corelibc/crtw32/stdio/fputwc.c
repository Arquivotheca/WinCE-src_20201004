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
*fputwc.c - write a wide character to an output stream
*
*
*Purpose:
*   defines fputwc() - writes a wide character to a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <crtmisc.h>
#include <stdio.h>
#include <stdlib.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>
#include <crttchar.h>

/***
*wint_t fputwc(ch, stream) - write a wide character to a stream
*
*Purpose:
*   Writes a wide character to a stream.  Function version of putwc().
*
*Entry:
*   wint_t ch - wide character to write
*   FILEX *stream - stream to write to
*
*Exit:
*   returns the wide character if successful
*   returns WEOF if fails
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl fputwc (
    wint_t ch,
    FILEX *str
    )
{
    REG1 FILEX *stream;
    REG2 wint_t retval;

    _VALIDATE_RETURN((str != NULL), EINVAL, WEOF);

    /* Init stream pointer */
    stream = str;

    if (!_lock_validate_str(stream))
        return WEOF;

    __STREAM_TRY
    {
        retval = (wint_t)_putwc_lk(ch, stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}

