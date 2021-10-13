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
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines ungetwc() - pushes a wide character back onto an input stream
*
*Revision History:
*       04-26-93  CFW   Module created.
*       04-30-93  CFW   Bring wide char support from ungetc.c.
*       05-10-93  CFW   Optimize, fix error handling.
*       06-02-93  CFW   Wide get/put use wint_t.
*       07-16-93  SRW   ALPHA Merge
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       10-01-93  CFW   Test only for TEXT, update comments.
*       10-28-93  CFW   Test for both IOSTRG and TEXT.
*       11-10-93  GJF   Merged in NT SDK version (picked up fix to a cast
*                       expression). Also replaced MTHREAD with _MT.
*       02-07-94  CFW   POSIXify.
*       08-31-94  CFW   Fix for "C" locale, call wctomb().
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       06-12-95  GJF   Replaced _osfile[] with _osfile() (macro referencing
*                       field in ioinfo struct).
*       07-28-95  GJF   Replaced _osfile() with _osfile_safe().
*       03-02-98  GJF   Exception-safe locking.
*       11-05-08  GJF   Don't push back characters onto strings (i.e., when
*                       called by swscanf).
*       12-16-99  GB    Modified for the case when return value from wctomb is
*                       greater then 2.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       12-02-03  SJ    Reroute Unicode I/O
*       04-07-04  AC    Use wctomb_s instead of wctomb
*                       VSW#246776
*       07-19-04  AC    Added _SAFECRT_IMPL for safecrt.lib
*       07-30-04  AC    Removed _SAFECRT_IMPL #if and moved the ungetw_nolock code
*                       into ungetc_nolock.inl
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <stdio.h>
#include <stdlib.h>
#include <file2.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>
#include <errno.h>
#include <wchar.h>
#include <setlocal.h>

/***
*wint_t ungetwc(ch, stream) - put a wide character back onto a stream
*
*Purpose:
*       Guaranteed one char pushback on a stream as long as open for reading.
*       More than one char pushback in a row is not guaranteed, and will fail
*       if it follows an ungetc which pushed the first char in buffer. Failure
*       causes return of WEOF.
*
*Entry:
*       wint_t ch - wide character to push back
*       FILE *stream - stream to push character onto
*
*Exit:
*       returns ch
*       returns WEOF if tried to push WEOF, stream not opened for reading or
*       or if we have already ungetc'd back to beginning of buffer.
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl ungetwc (
        wint_t ch,
        FILE *stream
        )
{
        wint_t retval;

        _VALIDATE_RETURN( (stream != NULL), EINVAL, WEOF);
        _CHECK_IO_INIT(WEOF);

        _lock_str(stream);

        __try {
                retval = _ungetwc_nolock (ch, stream);
        }
        __finally {
                _unlock_str(stream);
        }

        return(retval);
}

#include <ungetwc_nolock.inl>

#endif /* _POSIX_ */
