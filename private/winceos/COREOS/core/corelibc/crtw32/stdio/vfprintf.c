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
*vfprintf.c - fprintf from variable arg list
*
*Purpose:
*   defines vfprintf() - print formatted output, but take args from
*   a stdargs pointer.
*
*******************************************************************************/

#include <cruntime.h>
#include <crttchar.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

/***
*int vfprintf(stream, format, ap) - print to file from varargs
*
*Purpose:
*   Performs formatted output to a file.  The arg list is a variable
*   argument list pointer.
*
*Entry:
*   FILEX *stream - stream to write data to
*   CRT_TCHAR *format - format string containing data format
*   va_list ap - variable arg list pointer
*
*Exit:
*   returns number of correctly output characters
*   returns negative number if error occurred
*
*Exceptions:
*
*******************************************************************************/

#ifdef CRT_UNICODE
#define _vftprintf_helper vfwprintf_helper
#else
#define _vftprintf_helper vfprintf_helper
#endif

static int __cdecl _vftprintf_helper (
    FILEX *str,
    const CRT_TCHAR *format,
    va_list ap,
    BOOL fFormatValidations
    )
/*
 * 'V'ariable argument 'F'ile (stream) 'PRINT', 'F'ormatted
 */
{
    REG1 FILEX *stream;
    REG2 int buffing;
    REG3 int retval;

    _VALIDATE_RETURN((str != NULL), EINVAL, -1);
    _VALIDATE_RETURN((format != NULL), EINVAL, -1);

    /* Init stream pointer */
    stream = str;

    if (!_lock_validate_str(stream))
        return 0;

    __STREAM_TRY
    {
        buffing = _stbuf(stream);
        retval = _toutput(stream, format, ap, fFormatValidations);
        _ftbuf(buffing, stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}

int __cdecl _vftprintf(FILEX *str, const CRT_TCHAR *format, va_list ap)
{
    return _vftprintf_helper(str, format, ap, FALSE);
}

int __cdecl _vftprintf_s(FILEX *str, const CRT_TCHAR *format, va_list ap)
{
    return _vftprintf_helper(str, format, ap, TRUE);
}

int __cdecl _vtprintf(const CRT_TCHAR *format, va_list ap)
{
    /* Initialize before using *internal* stdout */
    if(!CheckStdioInit())
        return 0;

    return _vftprintf_helper(stdout, format, ap, FALSE);
}

int __cdecl _vtprintf_s(const CRT_TCHAR *format, va_list ap)
{
    /* Initialize before using *internal* stdout */
    if(!CheckStdioInit())
        return 0;

    return _vftprintf_helper(stdout, format, ap, TRUE);
}

int __cdecl _ftprintf(FILEX *str, const CRT_TCHAR *format, ...)
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _vftprintf_helper(str, format, arglist, FALSE);
    va_end(arglist);
    return retval;
}

int __cdecl _ftprintf_s(FILEX *str, const CRT_TCHAR *format, ...)
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _vftprintf_helper(str, format, arglist, TRUE);
    va_end(arglist);
    return retval;
}

int __cdecl _tprintf(const CRT_TCHAR *format, ...)
{
    int retval;
    va_list arglist;

    /* Initialize before using *internal* stdout */
    if(!CheckStdioInit())
        return 0;

    va_start(arglist, format);
    retval = _vftprintf_helper(stdout, format, arglist, FALSE);
    va_end(arglist);
    return retval;
}

int __cdecl _tprintf_s(const CRT_TCHAR *format, ...)
{
    int retval;
    va_list arglist;

    /* Initialize before using *internal* stdout */
    if(!CheckStdioInit())
        return 0;

    va_start(arglist, format);
    retval = _vftprintf_helper(stdout, format, arglist, TRUE);
    va_end(arglist);
    return retval;
}

