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
*wscanf.c - read formatted data from stdin
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines wscanf() - reads formatted data from stdin
*
*Revision History:
*       05-16-92  KRS   Created from scanf.c.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   Use _[un]lock_str2 instead of _[un]lock_str. Also,
*                       removed useless local and macro.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-23-03  SJ    Secure Version for scanf family which takes an extra 
*                       size parameter from the var arg list.
*       03-23-05  MSL   Review comment cleanup
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

/***
*int vwscanf(format, ...)
*
*Purpose:
*       This is a helper function to be called from wscanf & wscanf_s
*
*Entry:
*       char *format - format string
*       va_list arglist - arglist of output pointers
*
*Exit:
*       returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/

int __cdecl vwscanf (
        WINPUTFN winputfn,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list arglist
        )
/*
 * stdin 'W'char_t 'SCAN', 'F'ormatted
 */
{
        int retval;

        _VALIDATE_RETURN( (format != NULL), EINVAL, EOF);

        _lock_str2(0, stdin);
        __try {
            retval = (winputfn(stdin, format, plocinfo, arglist));
        }
        __finally {
            _unlock_str2(0, stdin);
        }

        return(retval);
}

/***
*int wscanf(format, ...) - read formatted data from stdin
*
*Purpose:
*       Reads formatted data from stdin into arguments.  _input does the real
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
int __cdecl wscanf (
        const wchar_t *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return vwscanf(_winput_l, format, NULL, arglist);
}

int __cdecl _wscanf_l (
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return vwscanf(_winput_l, format, plocinfo, arglist);
}
        
/***
*int wscanf_s(format, ...)
*
*   Same as wscanf above except that it calls _input_s to do the real work.
*   _input_s has a size check for array parameters.
*
*******************************************************************************/
int __cdecl wscanf_s (
        const wchar_t *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return vwscanf(_winput_s_l, format, NULL, arglist);
}

int __cdecl _wscanf_s_l (
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return vwscanf(_winput_s_l, format, plocinfo, arglist);
}

#endif /* _POSIX_ */
