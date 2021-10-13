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
*fprintf.c - print formatted data to stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fprintf() - print formatted data to stream
*
*Revision History:
*       09-02-83  RN    initial version
*       04-13-87  JCR   added const to declaration
*       06-24-87  JCR   (1) Made declaration conform to ANSI prototype and use
*                       the va_ macros; (2) removed SS_NE_DS conditionals.
*       11-05-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-27-88  PHG   Merged DLL and normal versions
*       06-14-88  JCR   Use near pointer to reference _iob[] entries
*       08-25-88  GJF   Don't use FP_OFF() macro for the 386
*       08-17-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-15-90  GJF   Fixed copyright
*       03-19-90  GJF   Made calling type _CALLTYPE2, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-02-90  GJF   New-style function declarator.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       02-27-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       12-02-03  SJ    Reroute Unicode I/O
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-13-04  SJ    VSW#242637 - removing _printf_p from the headers
*       03-13-04  MSL   Avoid returning from __try for prefast
*       03-18-04  SJ    The fns are back in the headers, don't need the #define.
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
*int fprintf(stream, format, ...) - print formatted data
*
*Purpose:
*       Prints formatted data on the given using the format string to
*       format data and getting as many arguments as called for
*       _output does the real work here
*
*Entry:
*       FILE *stream - stream to print on
*       char *format - format string to control data format/number of arguments
*       followed by arguments to print, number and type controlled by
*       format string
*
*Exit:
*       returns number of characters printed
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fprintf (
        FILE *str,
        const char *format,
        ...
        )
/*
 * 'F'ile (stream) 'PRINT', 'F'ormatted
 */
{
    va_list(arglist);
    FILE *stream;
    int buffing;
    int retval=0;

    _VALIDATE_RETURN( (str != NULL), EINVAL, -1);
    _VALIDATE_RETURN( (format != NULL), EINVAL, -1);
    _CHECK_IO_INIT(-1);

    va_start(arglist, format);

    /* Init stream pointer */
    stream = str;

    _lock_str(stream);
    __try {
        _VALIDATE_STREAM_ANSI_SETRET(stream, EINVAL, retval, -1);

        if (retval==0)
        {
            buffing = _stbuf(stream);
            retval = _output_l(stream,format,NULL,arglist);
            _ftbuf(buffing, stream);
        }
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}

int __cdecl _fprintf_l (
        FILE *str,
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vfprintf_l(str, format, plocinfo, arglist);
}        

int __cdecl _fprintf_s_l (
        FILE *str,
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vfprintf_s_l(str, format, plocinfo, arglist);
}        

int __cdecl fprintf_s (
        FILE *str,
        const char *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vfprintf_s_l(str, format, NULL, arglist);
}        

int __cdecl _fprintf_p_l (
        FILE *str,
        const char *format,
        _locale_t plocinfo,                
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vfprintf_p_l(str, format, plocinfo, arglist);
}        

int __cdecl _fprintf_p (
        FILE *str,
        const char *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vfprintf_p_l(str, format, NULL, arglist);
}        
