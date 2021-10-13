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
*sscanf.c - read formatted data from string
*
*Purpose:
*   defines scanf() - reads formatted data from string
*
*******************************************************************************/

#include <cruntime.h>
#include <crttchar.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <string.h>
#include <internal.h>
#include <mtdll.h>

/***
*int sscanf(string, format, ...) - read formatted data from string
*
*Purpose:
*   Reads formatted data from string into arguments.  _input does the real
*   work here.  Sets up a FILEX so file i/o operations can be used, makes
*   string look like a huge buffer to it, but _filbuf will refuse to refill
*   it if it is exhausted.
*
*   Allocate the 'fake' _iob[] entryit statically instead of on
*   the stack so that other routines can assume that _iob[] entries are in
*   are in DGROUP and, thus, are near.
*
*   Multi-thread: (1) Since there is no stream, this routine must never try
*   to get the stream lock (i.e., there is no stream lock either).  (2)
*   Also, since there is only one staticly allocated 'fake' iob, we must
*   lock/unlock to prevent collisions.
*
*Entry:
*   char *string - string to read data from
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
#define _stscanf_helper swscanf_helper
#else
#define _stscanf_helper sscanf_helper
#endif

static int __cdecl _stscanf_helper (
    REG2 const CRT_TCHAR *string,
    const CRT_TCHAR *format,
    va_list ap,
    BOOL fSecureScanf
    )
/*
 * 'S'tring 'SCAN', 'F'ormatted
 */
{
         FILEX str;
    REG1 FILEX *infile = &str;
    REG2 int retval;

    _VALIDATE_RETURN((string != NULL), EINVAL, EOF);
    _VALIDATE_RETURN((format != NULL), EINVAL, EOF);

    infile->_flag = _IOREAD|_IOSTRG|_IOMYBUF;
    infile->_ptr = infile->_base = (char *)string;
    infile->_cnt = _tcslen(string)*sizeof(CRT__TCHAR);

    retval = _tinput(infile, (const CRT__TUCHAR *)format, ap, fSecureScanf);

    return(retval);
}

int __cdecl _stscanf (
    const CRT_TCHAR *string,
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _stscanf_helper(string, format, arglist, FALSE);
    va_end(arglist);
    return retval;
}

int __cdecl _stscanf_s (
    const CRT_TCHAR *string,
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _stscanf_helper(string, format, arglist, TRUE);
    va_end(arglist);
    return retval;
}

