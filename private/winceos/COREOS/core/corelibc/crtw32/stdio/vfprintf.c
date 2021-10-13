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
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines vfprintf() - print formatted output, but take args from
*       a stdargs pointer.
*
*Revision History:
*       09-02-83  RN    original fprintf
*       06-17-85  TC    rewrote to use new varargs macros, and to be vfprintf
*       04-13-87  JCR   added const to declaration
*       11-06-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-31-88  PHG   Merged DLL and normal versions
*       06-15-88  JCR   Near reference to _iob[] entries; improve REG variables
*       08-25-88  GJF   Don't use FP_OFF() macro for the 386
*       08-18-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-16-90  GJF   Fixed copyright
*       03-20-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-25-90  SBM   Replaced <assertm.h> by <assert.h>, <varargs.h> by
*                       <stdarg.h>
*       10-03-90  GJF   New-style function declarator.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       12-02-03  SJ    Reroute Unicode I/O
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       03-13-04  MSL   Avoid returning from __try for prefast
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

/***
*int vfprintf(stream, format, ap) - print to file from varargs
*
*Purpose:
*       Performs formatted output to a file.  The arg list is a variable
*       argument list pointer.
*
*Entry:
*       FILE *stream - stream to write data to
*       char *format - format string containing data format
*       va_list ap - variable arg list pointer
*
*Exit:
*       returns number of correctly output characters
*       returns negative number if error occurred
*
*Exceptions:
*
*******************************************************************************/

int __cdecl vfprintf_helper (
        OUTPUTFN outfn,
        FILE *str,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
/*
 * 'V'ariable argument 'F'ile (stream) 'PRINT', 'F'ormatted
 */
{
        FILE *stream;
        int buffing;
        int retval=0;

        _VALIDATE_RETURN( (str != NULL), EINVAL, -1);
        _VALIDATE_RETURN( (format != NULL), EINVAL, -1);
        _CHECK_IO_INIT(-1);

        /* Init stream pointer */
        stream = str;

        _lock_str(stream);
        __try {

        _VALIDATE_STREAM_ANSI_SETRET(stream, EINVAL, retval, -1);
		if(retval==0)
		{
			buffing = _stbuf(stream);
			retval = outfn(stream,format,plocinfo, ap );
			_ftbuf(buffing, stream);
		}

        }
        __finally {
            _unlock_str(stream);
        }

        return(retval);
}

int __cdecl _vfprintf_l (
        FILE *str,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vfprintf_helper(_output_l, str, format, plocinfo, ap);
}

int __cdecl _vfprintf_s_l (
        FILE *str,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vfprintf_helper(_output_s_l, str, format, plocinfo, ap);
}

int __cdecl _vfprintf_p_l (
        FILE *str,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    return vfprintf_helper(_output_p_l, str, format, plocinfo, ap);
}

int __cdecl vfprintf (
        FILE *str,
        const char *format,
        va_list ap
        )
{
    return vfprintf_helper(_output_l, str, format, NULL, ap);
}

int __cdecl vfprintf_s (
        FILE *str,
        const char *format,
        va_list ap
        )
{
    return vfprintf_helper(_output_s_l, str, format, NULL, ap);
}

int __cdecl _vfprintf_p (
        FILE *str,
        const char *format,
        va_list ap
        )
{
    return vfprintf_helper(_output_p_l, str, format, NULL, ap);
}
