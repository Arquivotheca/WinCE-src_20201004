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
*fgetwc.c - get a wide character from a stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fgetwc() - read a wide character from a stream
*
*Revision History:
*       04-26-93  CFW   Module created.
*       04-30-93  CFW   Bring wide char support from fgetc.c.
*       05-03-93  CFW   Add getwc function.
*       05-10-93  CFW   Optimize, fix error handling.
*       06-02-93  CFW   Wide get/put use wint_t.
*       09-14-93  CFW   Fix EOF cast bug.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       10-01-93  CFW   Test only for TEXT.
*       10-22-93  CFW   Test for invalid MB chars using global preset flag.
*       10-28-93  CFW   Test for both IOSTRG and TEXT.
*       02-07-94  CFW   POSIXify.
*       08-31-94  CFW   Fix for "C" locale, call mbtowc().
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       06-12-95  GJF   Replaced _osfile[] with _osfile() (macro referencing
*                       field in ioinfo struct).
*       07-28-95  GJF   Replaced _osfile() with _osfile_safe().
*       04-18-97  JWM   Explicit cast added to avoid new C4242 warnings.
*       02-27-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       12-02-03  SJ    Reroute Unicode I/O
*       07-19-04  AC    Added _SAFECRT_IMPL for safecrt.lib
*       07-30-04  AC    Removed _SAFECRT_IMPL #if and moved the fgetwc_nolock code
*                       into fgetwc_nolock.inl
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <stdio.h>
#include <stdlib.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>
#include <errno.h>
#include <wchar.h>
#include <tchar.h>
#include <setlocal.h>

/***
*wint_t fgetwc(stream) - read a wide character from a stream
*
*Purpose:
*       reads a wide character from the given stream
*
*Entry:
*       FILE *stream - stream to read wide character from
*
*Exit:
*       returns the wide character read
*       returns WEOF if at end of file or error occurred
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl fgetwc (
        FILE *stream
        )
{
    wint_t retval;

    _VALIDATE_RETURN( (stream != NULL), EINVAL, WEOF);

    _lock_str(stream);
    __try {
        retval = _getwc_nolock(stream);
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}


#include <fgetwc_nolock.inl>

#undef getwc

wint_t __cdecl getwc (
        FILE *stream
        )
{
    return fgetwc(stream);
}

#endif /* _POSIX_ */
