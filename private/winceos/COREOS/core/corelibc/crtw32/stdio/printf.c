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
*printf.c - print formatted
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines printf() - print formatted data
*
*Revision History:
*       09-02-83  RN    initial version
*       04-13-87  JCR   added const to declaration
*       06-24-87  JCR   (1) Made printf conform to ANSI prototype and use the
*                       va_ macros; (2) removed SS_NE_DS conditionals.
*       11-04-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-27-88  PHG   Merged DLL and normal versions
*       06-14-88  JCR   Use near pointer to reference _iob[] entries
*       08-17-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-15-90  GJF   Fixed copyright
*       03-19-90  GJF   Made calling type _CALLTYPE2, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-03-90  GJF   New-style function declarator.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-07-95  GJF   Use _[un]lock_str2 instead of _[un]lock_str. Also,
*                       removed useless local and macros.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-13-04  SJ    VSW#242637 - removing _printf_p from the headers
*       03-18-04  SJ    The fns are back in the headers, don't need the #define.
*       10-01-04  AGH   Implement _[get|set]_printf_count_output
*       06-11-05  PML   __security_cookie might be 0
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <stddef.h>
#include <process.h>

/***
*int printf(format, ...) - print formatted data
*
*Purpose:
*       Prints formatted data on stdout using the format string to
*       format data and getting as many arguments as called for
*       Uses temporary buffering to improve efficiency.
*       _output does the real work here
*
*Entry:
*       char *format - format string to control data format/number of arguments
*       followed by list of arguments, number and type controlled by
*       format string
*
*Exit:
*       returns number of characters printed
*
*Exceptions:
*
*******************************************************************************/

int __cdecl printf (
        const char *format,
        ...
        )
/*
 * stdout 'PRINT', 'F'ormatted
 */
{
    va_list arglist;
    int buffing;
    int retval;

    _VALIDATE_RETURN( (format != NULL), EINVAL, -1);

    va_start(arglist, format);

    _lock_str2(1, stdout);
    __try {
        buffing = _stbuf(stdout);

        retval = _output_l(stdout,format,NULL,arglist);

        _ftbuf(buffing, stdout);

    }
    __finally {
        _unlock_str2(1, stdout);
    }

    return(retval);
}

int __cdecl _printf_l (
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
  
    va_start(arglist, plocinfo);

    return _vprintf_l(format, plocinfo, arglist);
}


int __cdecl _printf_s_l (
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vprintf_s_l(format, plocinfo, arglist);
}

int __cdecl printf_s (
        const char *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vprintf_s_l(format, NULL, arglist);
}

int __cdecl _printf_p_l (
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vprintf_p_l(format, plocinfo, arglist);
}

int __cdecl _printf_p (
        const char *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vprintf_p_l(format, NULL, arglist);
}

static UINT_PTR __enable_percent_n = 0;

/***
*int _set_printf_count_output(int)
*
*Purpose:
*   Enables or disables %n format specifier for printf family functions
*
*Internals:
*   __enable_percent_n is set to (__security_cookie|1) for security reasons;
*   if set to a static value, an attacker could first modify __enable_percent_n
*   and then provide a malicious %n specifier.  The cookie is ORed with 1
*   because a zero cookie is a possibility.
******************************************************************************/
int __cdecl _set_printf_count_output(int value)
{
    int old = (__enable_percent_n == (__security_cookie | 1));
    __enable_percent_n = (value ? (__security_cookie | 1) : 0);
    return old;
}

/***
*int _get_printf_count_output()
*
*Purpose:
*   Checks whether %n format specifier for printf family functions is enabled
******************************************************************************/
int __cdecl _get_printf_count_output()
{
    return ( __enable_percent_n == (__security_cookie | 1));
}
