//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*fwscanf.c - read formatted data from stream
*
*
*Purpose:
*   defines fwscanf() - reads formatted data from stream
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
*int fwscanf(stream, format, ...) - read formatted data from stream
*
*Purpose:
*   Reads formatted data from stream into arguments.  _input does the real
*   work here.
*
*Entry:
*   FILEX *stream - stream to read data from
*   wchar_t *format - format string
*   followed by list of pointers to storage for the data read.  The number
*   and type are controlled by the format string.
*
*Exit:
*   returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fwscanf (
    FILEX *stream,
    const wchar_t *format,
    ...
    )
/*
 * 'F'ile (stream) 'W'char_t 'SCAN', 'F'ormatted
 */
{
    int retval;
    va_list arglist;

    if(!CheckStdioInit())
        return 0;

    va_start(arglist, format);

    _ASSERTE(stream != NULL);
    _ASSERTE(format != NULL);

    if (! _lock_validate_str(stream))
        return 0;

    retval = (_winput(stream,format,arglist));
    _unlock_str(stream);

    return(retval);
}
