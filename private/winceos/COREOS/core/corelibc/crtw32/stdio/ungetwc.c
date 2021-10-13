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
*ungetwc.c - unget a wide character from a stream
*
*
*Purpose:
*   defines ungetwc() - pushes a wide character back onto an input stream
*
*******************************************************************************/

#include <cruntime.h>
#include <crtmisc.h>
#include <stdio.h>
#include <stdlib.h>
#include <file2.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>

/***
*wint_t ungetwc(ch, stream) - put a wide character back onto a stream
*
*Purpose:
*   Guaranteed one char pushback on a stream as long as open for reading.
*   More than one char pushback in a row is not guaranteed, and will fail
*   if it follows an ungetc which pushed the first char in buffer. Failure
*   causes return of WEOF.
*
*Entry:
*   wint_t ch - wide character to push back
*   FILEX *stream - stream to push character onto
*
*Exit:
*   returns ch
*   returns WEOF if tried to push WEOF, stream not opened for reading or
*   or if we have already ungetc'd back to beginning of buffer.
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl ungetwc (
    REG2 wint_t ch,
    REG1 FILEX *stream
    )
{
    wint_t retval;

    _VALIDATE_RETURN((stream != NULL), EINVAL, WEOF);

    if (!_lock_validate_str(stream))
        return WEOF;

    __STREAM_TRY
    {
        retval = _ungetwc_lk (ch, stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}

