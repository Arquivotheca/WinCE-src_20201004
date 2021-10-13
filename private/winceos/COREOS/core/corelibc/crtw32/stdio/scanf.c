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
*scanf.c - read formatted data from stdin
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines scanf() - reads formatted data from stdin
*
*Revision History:
*       09-02-83  RN    initial version
*       04-13-87  JCR   added const to declaration
*       06-24-87  JCR   (1) Made declaration conform to ANSI prototype and use
*                       the va_ macros; (2) removed SS_NE_DS conditionals.
*       11-04-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-27-88  PHG   Merged DLL and normal versions
*       06-15-88  JCR   Near reference to _iob[] entries; improve REG variables
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
*       03-07-95  GJF   Use _[un]lock_str2 instead of _[un]lock_str. Also,
*                       removed useless local and macro.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-23-03  SJ    Secure Version for scanf family which takes an extra 
*                       size parameter from the var arg list.
*       10-31-04  MSL   Fix varargs bug
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

/***
*int vscanf(format, ...) - read formatted data from stdin
*
*Purpose:
*       This is a helper function to be called from fscanf & fscanf_s
*
*Entry:
*       INPUTFN inputfn - scanf & scanf_s pass either _input_l or _input_s_l
*                   which is then used to do the real work.            
*       char *format - format string
*       va_list arglist - arglist of output pointers
*
*Exit:
*       returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/

int __cdecl vscanf (
        INPUTFN inputfn,
        const char *format,
        _locale_t plocinfo,
        va_list arglist
        )
/*
 * stdin 'SCAN', 'F'ormatted
 */
{
    int retval;

    _VALIDATE_RETURN( (format != NULL), EINVAL, EOF);

    _lock_str2(0, stdin);
    __try {
        retval = (inputfn(stdin, format, plocinfo, arglist));
    }
    __finally {
        _unlock_str2(0, stdin);
    }

    return(retval);
}

/***
*int scanf(format, ...) - read formatted data from stdin
*
*Purpose:
*       Reads formatted data from stdin into arguments.  _input_l does the real
*       work here.
*
*Entry:
*       char *format - format string
*       followed by list of pointers to storage for the data read.  The number
*       and type are controlled by the format string.
*
*Exit:
*       returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/
int __cdecl scanf (
        const char *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return vscanf(_input_l, format, NULL, arglist);
}

int __cdecl _scanf_l (
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return vscanf(_input_l, format, plocinfo, arglist);
}

/***
*int scanf_s(format, ...) - read formatted data from stdin
*
*   Same as scanf above except that it calls _input_s_l to do the real work.
*   _input_s_l has a size check for array parameters.
*
*******************************************************************************/
int __cdecl scanf_s (
        const char *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return vscanf(_input_s_l, format, NULL, arglist);
}

int __cdecl _scanf_s_l (
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return vscanf(_input_s_l, format, plocinfo, arglist);
}
