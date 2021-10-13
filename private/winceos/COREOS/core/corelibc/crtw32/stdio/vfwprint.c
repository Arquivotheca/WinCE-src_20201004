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
*vfwprintf.c - fwprintf from variable arg list
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines vfwprintf() - print formatted output, but take args from
*       a stdargs pointer.
*
*Revision History:
*       05-16-92  KRS   Created from vfprintf.c.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <stdio.h>
#include <wchar.h>
#include <dbgint.h>
#include <stdarg.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <stddef.h>

/***
*int vfwprintf(stream, format, ap) - print to file from varargs
*
*Purpose:
*       Performs formatted output to a file.  The arg list is a variable
*       argument list pointer.
*
*Entry:
*       FILE *stream - stream to write data to
*       wchar_t *format - format string containing data format
*       va_list ap - variable arg list pointer
*
*Exit:
*       returns number of correctly output wide characters
*       returns negative number if error occurred
*
*Exceptions:
*
*******************************************************************************/

int __cdecl vfwprintf_helper (
        WOUTPUTFN woutfn,
        FILE *str,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
/*
 * 'V'ariable argument 'F'ile (stream) 'W'char_t 'PRINT', 'F'ormatted
 */
{
        FILE *stream;
        int buffing;
        int retval;

        _VALIDATE_RETURN( (str != NULL), EINVAL, -1);
        _VALIDATE_RETURN( (format != NULL), EINVAL, -1);

        /* Init stream pointer */
        stream = str;

        _lock_str(stream);
        __try {

        buffing = _stbuf(stream);
        retval = woutfn(stream,format,plocinfo,ap );
        _ftbuf(buffing, stream);

        }
        __finally {
            _unlock_str(stream);
        }

        return(retval);
}

int __cdecl _vfwprintf_l (
        FILE *str,
        const wchar_t *format,
        _locale_t plocinfo,        
        va_list ap
        )
{
    return vfwprintf_helper(_woutput_l, str, format, plocinfo, ap);
}

int __cdecl _vfwprintf_s_l (
        FILE *str,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vfwprintf_helper(_woutput_s_l, str, format, plocinfo, ap);
}

int __cdecl _vfwprintf_p_l (
        FILE *str,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vfwprintf_helper(_woutput_p_l, str, format, plocinfo, ap);
}

int __cdecl vfwprintf (
        FILE *str,
        const wchar_t *format,
        va_list ap
        )
{
    return vfwprintf_helper(_woutput_l, str, format, NULL, ap);
}

int __cdecl vfwprintf_s (
        FILE *str,
        const wchar_t *format,
        va_list ap
        )
{
    return vfwprintf_helper(_woutput_s_l, str, format, NULL, ap);
}

int __cdecl _vfwprintf_p (
        FILE *str,
        const wchar_t *format,
        va_list ap
        )
{
    return vfwprintf_helper(_woutput_p_l, str, format, NULL, ap);
}

#endif /* _POSIX_ */
