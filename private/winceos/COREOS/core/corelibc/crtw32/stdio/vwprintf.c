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
*vwprintf.c - wprintf from a var args pointer
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines vwprintf() - print formatted data from an argument list pointer
*
*Revision History:
*       05-16-92  KRS   Created from vprintf.c
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
#include <internal.h>
#include <file2.h>
#include <mtdll.h>
#include <stddef.h>

/***
*int vwprintf(format, ap) - print formatted data from an argument list pointer
*
*Purpose:
*       Prints formatted data items to stdout.  Uses a pointer to a
*       variable length list of arguments instead of an argument list.
*
*Entry:
*       wchar_t *format - format string, describes data format to write
*       va_list ap - pointer to variable length arg list
*
*Exit:
*       returns number of wide characters written
*
*Exceptions:
*
*******************************************************************************/

int __cdecl vwprintf_helper (
        WOUTPUTFN woutfn,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
/*
 * stdout 'V'ariable, 'W'char_t 'PRINT', 'F'ormatted
 */
{
        FILE *stream = stdout;
        int buffing;
        int retval;

        _VALIDATE_RETURN( (format != NULL), EINVAL, -1);

        _lock_str(stream);
        __try {
        buffing = _stbuf(stream);
        retval = woutfn(stream, format, plocinfo, ap );
        _ftbuf(buffing, stream);

        }
        __finally {
            _unlock_str(stream);
        }

        return(retval);
}

int __cdecl _vwprintf_l (
        const wchar_t *format,
        _locale_t plocinfo,                
        va_list ap
        )
{
    return vwprintf_helper(_woutput_l, format, plocinfo, ap);
}

int __cdecl _vwprintf_s_l (
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vwprintf_helper(_woutput_s_l, format, plocinfo, ap);
}

int __cdecl _vwprintf_p_l (
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vwprintf_helper(_woutput_p_l, format, plocinfo, ap);
}

int __cdecl vwprintf (
        const wchar_t *format,
        va_list ap
        )
{
    return vwprintf_helper(_woutput_l, format, NULL, ap);
}

int __cdecl vwprintf_s (
        const wchar_t *format,
        va_list ap
        )
{
    return vwprintf_helper(_woutput_s_l, format, NULL, ap);
}

int __cdecl _vwprintf_p (
        const wchar_t *format,
        va_list ap
        )
{
    return vwprintf_helper(_woutput_p_l, format, NULL, ap);
}

#endif /* _POSIX_ */
