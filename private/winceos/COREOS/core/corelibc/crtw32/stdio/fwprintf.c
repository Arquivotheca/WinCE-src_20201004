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
*fwprintf.c - print formatted data to stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fwprintf() - print formatted data to stream
*
*Revision History:
*       05-16-92  KRS   Created from fprintf.c.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-13-04  SJ    VSW#242637 - removing _printf_p from the headers
*       03-18-04  SJ    The fns are back in the headers, don't need the #define.
*       03-23-05  MSL   Review comment cleanup
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <stddef.h>

/***
*int fwprintf(stream, format, ...) - print formatted data
*
*Purpose:
*       Prints formatted data on the given using the format string to
*       format data and getting as many arguments as called for
*       _output does the real work here
*
*Entry:
*       FILE *stream - stream to print on
*       wchar_t *format - format string to control data format/number of arguments
*       followed by arguments to print, number and type controlled by
*       format string
*
*Exit:
*       returns number of wide characters printed
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fwprintf (
        FILE *str,
        const wchar_t *format,
        ...
        )
/*
 * 'F'ile (stream) 'W'char_t 'PRINT', 'F'ormatted
 */
{
    va_list(arglist);
    FILE *stream;
    int buffing;
    int retval;

    _VALIDATE_RETURN( (str != NULL), EINVAL, -1);
    _VALIDATE_RETURN( (format != NULL), EINVAL, -1);

    va_start(arglist, format);

    /* Init stream pointer */
    stream = str;

    _lock_str(stream);
    __try {
        buffing = _stbuf(stream);
        retval = _woutput_l(stream,format,NULL,arglist);
        _ftbuf(buffing, stream);
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}

int __cdecl _fwprintf_l (
        FILE *str,
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vfwprintf_l(str, format, plocinfo, arglist);
}

int __cdecl _fwprintf_s_l (
        FILE *str,
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vfwprintf_s_l(str, format, plocinfo, arglist);
}

int __cdecl fwprintf_s (
        FILE *str,
        const wchar_t *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vfwprintf_s_l(str, format, NULL, arglist);
}

int __cdecl _fwprintf_p_l (
        FILE *str,
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vfwprintf_p_l(str, format, plocinfo, arglist);
}

int __cdecl _fwprintf_p (
        FILE *str,
        const wchar_t *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vfwprintf_p_l(str, format, NULL, arglist);
}

#endif /* _POSIX_ */
