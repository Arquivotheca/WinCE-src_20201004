//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*vswprint.c - print formatted data into a string from var arg list
*
*
*Purpose:
*   defines vswprintf() and _vsnwprintf() - print formatted output to
*   a string, get the data from an argument ptr instead of explicit
*   arguments.
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <internal.h>
#include <limits.h>
#include <mtdll.h>

#define MAXSTR ((INT_MAX / sizeof(wchar_t))-1)

/***
*ifndef _COUNT_
*int vswprintf(string, format, ap) - print formatted data to string from arg ptr
*else
*int _vsnwprintf(string, format, ap) - print formatted data to string from arg ptr
*endif
*
*Purpose:
*   Prints formatted data, but to a string and gets data from an argument
*   pointer.
*   Sets up a FILEX so file i/o operations can be used, make string look
*   like a huge buffer to it, but _flsbuf will refuse to flush it if it
*   fills up. Appends '\0' to make it a true string.
*
*   Allocate the 'fake' _iob[] entryit statically instead of on
*   the stack so that other routines can assume that _iob[] entries are in
*   are in DGROUP and, thus, are near.
*
*ifdef _COUNT_
*   The _vsnwprintf() flavor takes a count argument that is
*   the max number of bytes that should be written to the
*   user's buffer.
*endif
*
*   Multi-thread: (1) Since there is no stream, this routine must never try
*   to get the stream lock (i.e., there is no stream lock either).  (2)
*   Also, since there is only one staticly allocated 'fake' iob, we must
*   lock/unlock to prevent collisions.
*
*Entry:
*   wchar_t *string - place to put destination string
*ifdef _COUNT_
*   size_t count - max number of bytes to put in buffer
*endif
*   wchar_t *format - format string, describes format of data
*   va_list ap - varargs argument pointer
*
*Exit:
*   returns number of wide characters in string
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _vsnwprintf (
    wchar_t *string,
    size_t count,
    const wchar_t *format,
    va_list ap
    )
{
        FILEX str;
    REG1 FILEX *outfile = &str;
    REG2 int retval;

    _ASSERTE(string != NULL);
    _ASSERTE(format != NULL);

    outfile->_flag = _IOWRT|_IOSTRG;
    outfile->_ptr = outfile->_base = (char *) string;
    outfile->_cnt = count*sizeof(wchar_t);

    retval = _woutput(outfile,format,ap );

    _putc_lk('\0',outfile);     /* no-lock version */
    _putc_lk('\0',outfile);     /* 2nd byte for wide char version */

    return(retval);
}

int __cdecl vswprintf(wchar_t *string, const wchar_t *format, va_list ap)
{
    return _vsnwprintf(string, MAXSTR, format, ap);
}

int __cdecl _snwprintf(wchar_t *string, size_t count,const wchar_t *format, ...)
{
    va_list arglist;
    va_start(arglist, format);
    return _vsnwprintf(string, count, format, arglist);
}

int __cdecl swprintf(wchar_t *string, const wchar_t *format, ...)
{
    va_list arglist;
    va_start(arglist, format);
    return _vsnwprintf(string, MAXSTR, format, arglist);
}

//
// WIN32 version: limit to 1024 chars (doc says 1024 bytes, but it's acutally 1024 chars according
//                to the desktop behavior)
// NOTE: testing on the desktop shows that it can write up to 1025 chars to the buffer because it
//       null terminate the string.
//
#define MAX_W32_LEN     1024
int __cdecl wvsprintfW(wchar_t *string, const wchar_t *format, va_list ap)
{
    int len = _vsnwprintf(string, MAX_W32_LEN, format, ap);

    // _vsnwprintf returns negative number if buffer is not big enough
    if (MAX_W32_LEN <= (DWORD) len) {
        // null terminate the string
        string[len = MAX_W32_LEN] = 0;
    }
    return len;
}

int __cdecl wsprintfW(wchar_t *string, const wchar_t *format, ...)
{
    int len;
    va_list arglist;
    va_start(arglist, format);

    len = _vsnwprintf(string, MAX_W32_LEN, format, arglist);

    // _vsnwprintf returns negative number if buffer is not big enough
    if (MAX_W32_LEN <= (DWORD) len) {
        // null terminate the string
        string[len = MAX_W32_LEN] = 0;
    }
    return len;
}


