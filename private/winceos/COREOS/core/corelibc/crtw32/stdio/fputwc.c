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
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fputwc() - writes a wide character to a stream
*
*Revision History:
*       04-26-93  CFW   Module created.
*       04-30-93  CFW   Bring wide char support from fputc.c.
*       05-03-93  CFW   Add putwc function.
*       05-10-93  CFW   Optimize, fix error handling.
*       06-02-93  CFW   Wide get/put use wint_t.
*       07-16-93  SRW   ALPHA Merge
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       10-01-93  CFW   Test only for TEXT.
*       10-28-93  CFW   Test for both IOSTRG and TEXT.
*       11-05-93  GJF   Merged with NT SDK version (fix to a cast expr).
*       02-07-94  CFW   POSIXify.
*       08-31-94  CFW   Fix for "C" locale, call wctomb().
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       06-12-95  GJF   Replaced _osfile[] with _osfile() (macro referencing
*                       field in ioinfo struct).
*       07-28-95  GJF   Replaced _osfile() with _osfile_safe().
*       04-18-97  JWM   Explicit cast added to avoid new C4242 warnings.
*       02-27-98  GJF   Exception-safe locking.
*       12-16-99  GB    Modified for the case when return value from wctomb is
*                       greater then 2.
*       11-22-00  PML   Wide-char *putwc* functions take a wchar_t, not wint_t.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       12-02-03  SJ    Reroute Unicode I/O
*       04-07-04  AC    Use wctomb_s instead of wctomb
*                       VSW#246776
*       10-22-04  AGH   Make _fputwc_nolock pass wchars rather than chars into
*                       _flswbuf when operating on unicode files.
*       10-13-06  WL    Equally treat __IOINFO_TM_UTF16LE and __IOINFO_TM_UTF8
*                       DEVDIV#23854
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
*wint_t fputwc(ch, stream) - write a wide character to a stream
*
*Purpose:
*       Writes a wide character to a stream.  Function version of putwc().
*
*Entry:
*       wchar_t ch - wide character to write
*       FILE *stream - stream to write to
*
*Exit:
*       returns the wide character if successful
*       returns WEOF if fails
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl fputwc (
        wchar_t ch,
        FILE *str
        )
{
    FILE *stream;
    wint_t retval;

    _VALIDATE_RETURN((str != NULL), EINVAL, WEOF);
    _CHECK_IO_INIT(EINVAL);

    /* Init stream pointer */
    stream = str;

    _lock_str(stream);
    __try {
        retval = _fputwc_nolock(ch,stream);
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}

/***
*_fputwc_nolock() -  putwc() core routine (locked version)
*
*Purpose:
*       Core putwc() routine; assumes stream is already locked.
*
*       [See putwc() above for more info.]
*
*Entry: [See putwc()]
*
*Exit:  [See putwc()]
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl _fputwc_nolock (
        wchar_t ch,
        FILE *str
        )
{
#ifndef _NTSUBSET_
        if (!(str->_flag & _IOSTRG))
        { 
            if (_textmode_safe(_fileno(str)) == __IOINFO_TM_UTF16LE
                    || _textmode_safe(_fileno(str)) == __IOINFO_TM_UTF8)
            {
                /* binary (Unicode) mode */
                if ( (str->_cnt -= sizeof(wchar_t)) >= 0 ) {
                    return (wint_t) (0xffff & (*((wchar_t *)(str->_ptr))++ = (wchar_t)ch));
                } else {
                    return (wint_t) _flswbuf(ch, str);
                }
            }
            else if ((_osfile_safe(_fileno(str)) & FTEXT))
            {
                int size, i;
                char mbc[MB_LEN_MAX];
                
                /* text (multi-byte) mode */
                if (wctomb_s(&size, mbc, MB_LEN_MAX, ch) != 0)
                {
                        /*
                         * Conversion failed; errno is set by wctomb_s;
                         * we return WEOF to indicate failure.
                         */
                        return WEOF;
                }
                for ( i = 0; i < size; i++)
                {
                        if (_putc_nolock(mbc[i], str) == EOF)
                                return WEOF;
                }
                return (wint_t)(0xffff & ch);
            }
        }
#endif
        /* binary (Unicode) mode */
        if ( (str->_cnt -= sizeof(wchar_t)) >= 0 )
                return (wint_t) (0xffff & (*((wchar_t *)(str->_ptr))++ = (wchar_t)ch));
        else
                return (wint_t) _flswbuf(ch, str);
}

#undef putwc

wint_t __cdecl putwc (
        wchar_t ch,
        FILE *str
        )
{
        return fputwc(ch, str);
}

#endif /* _POSIX_ */
