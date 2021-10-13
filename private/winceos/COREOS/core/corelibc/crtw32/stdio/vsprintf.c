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
*vsprintf.c - print formatted data into a string from var arg list
*
*
*Purpose:
*   defines vsprintf() and _vsnprintf() - print formatted output to
*   a string, get the data from an argument ptr instead of explicit
*   arguments.
*
*******************************************************************************/

#include <cruntime.h>
#include <crttchar.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <internal.h>
#include <limits.h>
#include <mtdll.h>

#define MAXSTR (INT_MAX / sizeof(CRT_TCHAR))

/***
*ifndef _COUNT_
*int vsprintf(string, format, ap) - print formatted data to string from arg ptr
*else
*int _vsnprintf(string, format, ap) - print formatted data to string from arg ptr
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
*   The _vsnprintf() flavor takes a count argument that is
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
*   CRT_TCHAR *string - place to put destination string
*ifdef _COUNT_
*   size_t count - max number of bytes to put in buffer
*endif
*   CRT_TCHAR *format - format string, describes format of data
*   va_list ap - varargs argument pointer
*
*Exit:
*   returns number of characters in string
*
*Exceptions:
*
*******************************************************************************/

#ifdef CRT_UNICODE
#define _vsntprintf_helper vsnwprintf_helper
#endif

static int __cdecl _vsntprintf_helper_broken (
    CRT_TCHAR *string,
    size_t count,
    const CRT_TCHAR *format,
    va_list ap,
    BOOL fFormatValidations
    )
{
    FILEX str;
    REG1 FILEX *outfile = &str;
    REG2 int retval;

    _VALIDATE_RETURN((string != NULL), EINVAL, -1);
    _VALIDATE_RETURN((format != NULL), EINVAL, -1);

    outfile->_flag = _IOWRT|_IOSTRG;
    outfile->_ptr = outfile->_base = (char*)string;
    outfile->_cnt = count*sizeof(CRT_TCHAR);

    retval = _toutput(outfile, format, ap, fFormatValidations);

    /* This code is broken, but we can't fix it due to backward compatibility */
    /* See _vsntprintf_helper below for the correct code */
    _putc_lk('\0',outfile);
#ifdef CRT_UNICODE
    _putc_lk('\0',outfile);
#endif

    return(retval);
}

int __cdecl _vsntprintf (
    CRT_TCHAR *string,
    size_t count,
    const CRT_TCHAR *format,
    va_list ap
    )
{
    return _vsntprintf_helper_broken(string, count, format, ap, FALSE);
}

int __cdecl _vstprintf(CRT_TCHAR *string, const CRT_TCHAR *format, va_list ap)
{
    return _vsntprintf_helper_broken(string, MAXSTR, format, ap, FALSE);
}

int __cdecl _sntprintf (
    CRT_TCHAR *string,
    size_t count,
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _vsntprintf_helper_broken(string, count, format, arglist, FALSE);
    va_end(arglist);
    return retval;
}

int __cdecl _stprintf(CRT_TCHAR *string, const CRT_TCHAR *format, ...)
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _vsntprintf_helper_broken(string, MAXSTR, format, arglist, FALSE);
    va_end(arglist);
    return retval;
}

#ifdef CRT_UNICODE
#define _vsntprintf_helper vsnwprintf_helper
#define _vsntprintf_s_helper vsnwprintf_s_helper
#endif

static int __cdecl _vsntprintf_helper (
    CRT_TCHAR *string,
    size_t count,
    const CRT_TCHAR *format,
    va_list ap,
    BOOL fFormatValidations
    )
{
    FILEX str;
    REG1 FILEX *outfile = &str;
    REG2 int retval;

    _VALIDATE_RETURN((string != NULL), EINVAL, -1);
    _VALIDATE_RETURN((format != NULL), EINVAL, -1);

    outfile->_flag = _IOWRT|_IOSTRG;
    outfile->_ptr = outfile->_base = (char*)string;
    outfile->_cnt = count*sizeof(CRT_TCHAR);

    retval = _toutput(outfile, format, ap, fFormatValidations);

    if((retval >= 0) && (_putc_lk('\0',outfile) != EOF)
#ifdef CRT_UNICODE
        && (_putc_lk('\0',outfile) != EOF)
#endif
        )
    {
        return(retval);
    }

    string[count - 1] = 0;
    if (outfile->_cnt < 0)
    {
        /* the buffer was too small; we return -2 to indicate truncation */
        return -2;
    }
    return -1;
}

static int __cdecl _vsntprintf_s_helper (
        CRT_TCHAR *string,
        size_t sizeInChars,
        size_t count,
        const CRT_TCHAR *format,
        va_list ap
        )
{
    int retvalue = -1;

    /* validation section */
    _VALIDATE_RETURN(format != NULL, EINVAL, -1);
    if (count == 0 && string == NULL && sizeInChars == 0)
    {
        /* this case is allowed; nothing to do */
        return 0;
    }
    _VALIDATE_RETURN(string != NULL && sizeInChars > 0, EINVAL, -1);

    if (sizeInChars > count)
    {
        retvalue = _vsntprintf_helper(string, count + 1, format, ap, TRUE);
        if (retvalue == -2)
        {
            /* the string has been truncated, return -1 */
            _SECURECRT__FILL_STRING(string, sizeInChars, count + 1);
            return -1;
        }
    }
    else /* sizeInChars <= count */
    {
        retvalue = _vsntprintf_helper(string, sizeInChars, format, ap, TRUE);
        string[sizeInChars - 1] = 0;
        /* we allow truncation if count == _TRUNCATE */
        if (retvalue == -2 && count == _TRUNCATE)
        {
            return -1;
        }
    }

    if (retvalue < 0)
    {
        string[0] = 0;
        _SECURECRT__FILL_STRING(string, sizeInChars, 1);
        if (retvalue == -2)
        {
            _VALIDATE_RETURN(("Buffer too small", 0), ERANGE, -1);
        }
        return -1;
    }

    _SECURECRT__FILL_STRING(string, sizeInChars, retvalue + 1);

    return (retvalue < 0 ? -1 : retvalue);
}

int __cdecl _vsntprintf_s (
    CRT_TCHAR *string,
    size_t sizeInChars,
    size_t count,
    const CRT_TCHAR *format,
    va_list ap
    )
{
    return _vsntprintf_s_helper(string, sizeInChars, count, format, ap);
}

int __cdecl _vstprintf_s (
    CRT_TCHAR *string,
    size_t sizeInChars,
    const CRT_TCHAR *format,
    va_list ap
    )
{
    return _vsntprintf_s_helper(string, sizeInChars, MAXSTR, format, ap);
}

int __cdecl _sntprintf_s (
    CRT_TCHAR *string,
    size_t sizeInChars,
    size_t count,
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _vsntprintf_s_helper(string, sizeInChars, count, format, arglist);
    va_end(arglist);
    return retval;
}

int __cdecl _stprintf_s (
    CRT_TCHAR *string,
    size_t sizeInChars,
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _vsntprintf_s_helper(string, sizeInChars, MAXSTR, format, arglist);
    va_end(arglist);
    return retval;
}

int __cdecl _vsctprintf (
    const CRT_TCHAR *format,
    va_list ap
    )
{
    FILEX str;
    REG1 FILEX *outfile = &str;
    REG2 int retval;

    _VALIDATE_RETURN((format != NULL), EINVAL, -1);

    outfile->_cnt = MAXSTR;
    outfile->_flag = _IOWRT|_IOSTRG;
    outfile->_ptr = outfile->_base = NULL;

    retval = _toutput(outfile, format, ap, FALSE);

    return(retval);
}

int __cdecl _sctprintf (
    const CRT_TCHAR *format,
    ...
    )
{
    int retval;
    va_list arglist;
    va_start(arglist, format);
    retval = _vsctprintf(format, arglist);
    va_end(arglist);
    return retval;
}

