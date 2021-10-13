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
*
*Purpose:
*   defines scanf() - reads formatted data from stdin
*
*******************************************************************************/

#include <cruntime.h>
#include <crttchar.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <file2.h>
#include <mtdll.h>
#include <internal.h>

/***
*int scanf(format, ...) - read formatted data from stdin
*
*Purpose:
*   Reads formatted data from stdin into arguments.  _input does the real
*   work here.
*
*Entry:
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
#define _tscanf_helper wscanf_helper
#else
#define _tscanf_helper scanf_helper
#endif

int __cdecl _tscanf_helper (
    const CRT_TCHAR *format,
    va_list ap,
    BOOL fSecureScanf
    )
/*
 * stdin 'SCAN', 'F'ormatted
 */
{
    int retval;

    /* Initialize before using *internal* stdin */
    if(!CheckStdioInit())
        return 0;

    _VALIDATE_RETURN((format != NULL), EINVAL, EOF);

    _lock_str2(0, (FILEX*)stdin);

    __STREAM_TRY
    {
        retval = _tinput(stdin, (const CRT__TUCHAR *)format, ap, fSecureScanf);
    }
    __STREAM_FINALLY
    {
        _unlock_str2(0, (FILEX*)stdin);
    }

    return(retval);
}

int __cdecl _tscanf (
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _tscanf_helper(format, arglist, FALSE);
    va_end(arglist);
    return(retval);
}

int __cdecl _tscanf_s (
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _tscanf_helper(format, arglist, TRUE);
    va_end(arglist);
    return(retval);
}

