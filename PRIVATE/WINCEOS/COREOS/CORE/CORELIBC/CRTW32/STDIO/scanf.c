//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

int __cdecl scanf (
    const char *format,
    ...
    )
/*
 * stdin 'SCAN', 'F'ormatted
 */
{
    int retval;
    va_list arglist;

    if(!CheckStdioInit())
        return 0;

    va_start(arglist, format);

    _ASSERTE(format != NULL);

    _lock_str2(0, (FILEX*)stdin);

    retval = (_input(stdin,format,arglist));

    _unlock_str2(0, (FILEX*)stdin);

    return(retval);
}
