//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*swscanf.c - read formatted data from wide-character string
*
*
*Purpose:
*   defines _swscanf() - reads formatted data from wide-character string
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <string.h>
#include <internal.h>
#include <mtdll.h>

/***
*int swscanf(string, format, ...) - read formatted data from wide-char string
*
*Purpose:
*   Same as sscanf (described below) from reads from a wide-char string.
*
*   Reads formatted data from string into arguments.  _winput does the real
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
*   wchar_t *string - wide-character string to read data from
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

int __cdecl swscanf (
    REG2 const wchar_t *string,
    const wchar_t *format,
    ...
    )
{
    va_list arglist;
        FILEX str;
    REG1 FILEX *infile = &str;
    REG2 int retval;

    va_start(arglist, format);

    _ASSERTE(string != NULL);
    _ASSERTE(format != NULL);

    infile->_flag = _IOREAD|_IOSTRG|_IOMYBUF;
    infile->_ptr = infile->_base = (char *) string;
    infile->_cnt = wcslen(string)*sizeof(wchar_t);

    retval = (_winput(infile,format,arglist));

    return(retval);
}

