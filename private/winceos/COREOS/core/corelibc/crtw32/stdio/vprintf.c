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
*vprintf.c - printf from a var args pointer
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines vprintf() - print formatted data from an argument list pointer
*
*Revision History:
*       09-02-83  RN    original printf
*       06-17-85  TC    rewrote to use new varargs macros to be vprintf
*       04-13-87  JCR   added const to declaration
*       11-06-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-31-88  PHG   Merged DLL and normal versions
*       06-15-88  JCR   Near reference to _iob[] entries; improve REG variables
*       08-18-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-16-90  GJF   Fixed copyright
*       03-20-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-25-90  SBM   Replaced <assertm.h> by <assert.h>, <varargs.h> by
*                       <stdarg.h>
*       10-03-90  GJF   New-style function declarator.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <internal.h>
#include <file2.h>
#include <mtdll.h>
#include <stddef.h>

/***
*int vprintf(format, ap) - print formatted data from an argument list pointer
*
*Purpose:
*       Prints formatted data items to stdout.  Uses a pointer to a
*       variable length list of arguments instead of an argument list.
*
*Entry:
*       char *format - format string, describes data format to write
*       va_list ap - pointer to variable length arg list
*
*Exit:
*       returns number of characters written
*
*Exceptions:
*
*******************************************************************************/

int __cdecl vprintf_helper (
        OUTPUTFN outfn,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
/*
 * stdout 'V'ariable, 'PRINT', 'F'ormatted
 */
{
        FILE *stream = stdout;
        int buffing;
        int retval;

        _VALIDATE_RETURN( (format != NULL), EINVAL, -1);

        _lock_str(stream);
        __try {

        buffing = _stbuf(stream);
        retval = outfn(stream, format, plocinfo, ap );
        _ftbuf(buffing, stream);

        }
        __finally {
            _unlock_str(stream);
        }

        return(retval);
}

int __cdecl _vprintf_l (
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vprintf_helper(_output_l,format, plocinfo, ap);
}

int __cdecl _vprintf_s_l (
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vprintf_helper(_output_s_l,format, plocinfo, ap);
}

int __cdecl _vprintf_p_l (
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vprintf_helper(_output_p_l,format, plocinfo, ap);
}

int __cdecl vprintf (
        const char *format,
        va_list ap
        )
{
    return vprintf_helper(_output_l,format, NULL, ap);
}

int __cdecl vprintf_s (
        const char *format,
        va_list ap
        )
{
    return vprintf_helper(_output_s_l,format, NULL, ap);
}

int __cdecl _vprintf_p (
        const char *format,
        va_list ap
        )
{
    return vprintf_helper(_output_p_l,format, NULL, ap);
}
