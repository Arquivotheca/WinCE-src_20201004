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
*fscanf.c - read formatted data from stream
*
*
*Purpose:
*   defines fscanf() - reads formatted data from stream
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
*int fscanf(stream, format, ...) - read formatted data from stream
*
*Purpose:
*   Reads formatted data from stream into arguments.  _input does the real
*   work here.
*
*Entry:
*   FILEX *stream - stream to read data from
*   char *format - format string
*   followed by list of pointers to storage for the data read.  The number
*   and type are controlled by the format string.
*
*Exit:
*   returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/

#ifdef CRT_UNICODE
#define _ftscanf_helper fwscanf_helper
#else
#define _ftscanf_helper fscanf_helper
#endif

static int __cdecl _ftscanf_helper (
    FILEX *stream,
    const CRT_TCHAR *format,
    va_list ap,
    BOOL fSecureScanf
    )
/*
 * 'F'ile (stream) 'SCAN', 'F'ormatted
 */
{
    int retval;

    _VALIDATE_RETURN((stream != NULL), EINVAL, EOF);
    _VALIDATE_RETURN((format != NULL), EINVAL, EOF);

    if (!_lock_validate_str(stream))
        return 0;

    __STREAM_TRY
    {
        retval = _tinput(stream, (const CRT__TUCHAR *)format, ap, fSecureScanf);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}

int __cdecl _ftscanf (
    FILEX *stream,
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _ftscanf_helper(stream, format, arglist, FALSE);
    va_end(arglist);
    return retval;
}

int __cdecl _ftscanf_s (
    FILEX *stream,
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _ftscanf_helper(stream, format, arglist, TRUE);
    va_end(arglist);
    return retval;
}


