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
*fwscanf.c - read formatted data from stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fwscanf() - reads formatted data from stream
*
*Revision History:
*       05-16-92  KRS   Created from fscanf.c.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       10-23-03  SJ    Secure Version for scanf family which takes an extra 
*                       size parameter from the var arg list.
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
*int vfwscanf(stream, format, ...) - read formatted data from stream
*
*Purpose:
*       This is a helper function to be called from fwscanf & fwscanf_s
*
*Entry:
*       WINPUTFN winputfn - fwscanf & fwscanf_s pass either _winput or 
*       _winput_s which is then used to do the real work.            
*       FILE *stream - stream to read data from
*       wchar_t *format - format string
*       va_list arglist - arglist of output pointers
*
*Exit:
*       returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/

int __cdecl vfwscanf (
        WINPUTFN winputfn,
        FILE *stream,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list arglist
        )
/*
 * 'F'ile (stream) 'W'char_t 'SCAN', 'F'ormatted
 */
{
    int retval;

    _VALIDATE_RETURN((stream != NULL), EINVAL, EOF);
    _VALIDATE_RETURN((format != NULL), EINVAL, EOF);

    _lock_str(stream);
    __try {
        retval = (winputfn(stream, format, plocinfo, arglist));
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}

/***
*int fwscanf(stream, format, ...) - read formatted data from stream
*
*Purpose:
*       Reads formatted data from stream into arguments.  _input does the real
*       work here.
*
*Entry:
*       FILE *stream - stream to read data from
*       wchar_t *format - format string
*       followed by list of pointers to storage for the data read.  The number
*       and type are controlled by the format string.
*
*Exit:
*       returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/
int __cdecl fwscanf (
        FILE *stream,
        const wchar_t *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return vfwscanf(_winput_l, stream, format, NULL, arglist);
}

int __cdecl _fwscanf_l (
        FILE *stream,
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return vfwscanf(_winput_l, stream, format, plocinfo, arglist);
}

/***
*int fwscanf_s(stream, format, ...)
*
*   Same as fwscanf above except that it calls _winput_s to do the real work.
*   _winput_s has a size check for array parameters.
*
*Exceptions:
*
*******************************************************************************/
int __cdecl fwscanf_s (
        FILE *stream,
        const wchar_t *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return vfwscanf(_winput_s_l, stream, format, NULL, arglist);
}

int __cdecl _fwscanf_s_l (
        FILE *stream,
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return vfwscanf(_winput_s_l, stream, format, plocinfo, arglist);
}

#endif /* _POSIX_ */
