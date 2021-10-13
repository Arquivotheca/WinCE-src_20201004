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
*vswprint.c - print formatted data into a string from var arg list
*
*       Copyright (c) 1985-2001, Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines vswprintf(), _vswprintf_c and _vsnwprintf() - print formatted output to
*       a string, get the data from an argument ptr instead of explicit
*       arguments.
*
*Revision History:
*       05-16-92  KRS   Created from vsprintf.c.
*       02-18-93  SRW   Make FILE a local and remove lock usage.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*       09-05-94  SKS   Change "#ifdef" inside comments to "*ifdef" to avoid
*                       problems with CRTL source release process.
*       02-06-94  CFW   assert -> _ASSERTE.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       03-16-00  GB    Added _vscwprintf()
*       11-01-02  SJ    Fixed vswprintf functions for
*                       conformance vsWhidbey#2409
*       08-21-03  SJ    VSWhidbey#46431 - swprintf error return fix
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-02-04  AJS   VSWhidbey#231427 - allow zero-length buffer.
*       03-24-04  MSL   Fixed case where NULL, 0 is passed
*                       VSW#262109
*       04-08-04  AC    Added _snwprintf_s and _vsnwprintf_s
*                       VSW#263680
*       04-08-04  AC    Fixed behavior of swprintf when string exceeds count
*                       VSW#283508
*       08-05-04  AC    The secure versions call invalid_parameter if there is
*                       not enough space in the buffer.
*                       Added truncation behavior for _vsnprintf_s.
*                       Added string padding in debug.
*       09-22-04  AGH   Added _vscwprintf_p and _vscwprintf_helper
*       11-16-04  AC    Do not set errno to ERANGE in the truncation case
*                       VSW#411191
*       12-13-04  AC    Do not set string[count] to 0 with no reason
*                       VSW#422301
*       01-21-05  MSL   Fix integer overflow issue with output sizes
*       03-08-05  PAL   Add non-conformant implementation __vswprintf_l.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>

/* This is prevent pulling in the inline 
versions of (v)swprintf */
#define _INC_SWPRINTF_INL_

#include <stdio.h>
#include <wchar.h>
#include <dbgint.h>
#include <stdarg.h>
#include <internal.h>
#include <limits.h>
#include <mtdll.h>
#include <stddef.h>

#define MAXSTR INT_MAX


/***
*ifndef _COUNT_
*int _vswprintf(string, format, ap) - print formatted data to string from arg ptr
*else
*ifndef _SWPRINTFS_ERROR_RETURN_FIX
*int _vsnwprintf(string, cnt, format, ap) - print formatted data to string from arg ptr
*else
*int _vswprintf_c(string, cnt, format, ...) - print formatted data to string
*endif
*endif
*
*Purpose:
*       Prints formatted data, but to a string and gets data from an argument
*       pointer.
*       Sets up a FILE so file i/o operations can be used, make string look
*       like a huge buffer to it, but _flsbuf will refuse to flush it if it
*       fills up. Appends '\0' to make it a true string.
*
*       Allocate the 'fake' _iob[] entryit statically instead of on
*       the stack so that other routines can assume that _iob[] entries are in
*       are in DGROUP and, thus, are near.
*
*ifdef _COUNT_
*ifndef _SWPRINTFS_ERROR_RETURN_FIX
*       The _vsnwprintf() flavor takes a count argument that is
*       the max number of bytes that should be written to the
*       user's buffer.
*       We don't expose this function directly in the headers.
*else
*       The _vswprintf_c() flavor does the same thing as the _snwprintf
*       above, but, it also fixes a bug in the return value in the case
*       when there isn't enough space to write the null terminator
*       We don't fix this bug in _vsnwprintf because of backward 
*       compatibility. In new code, however, _vsnwprintf is #defined to
*       _vswprintf_c so users get the bugfix.
*
*endif
*
*       Multi-thread: (1) Since there is no stream, this routine must never try
*       to get the stream lock (i.e., there is no stream lock either).  (2)
*       Also, since there is only one staticly allocated 'fake' iob, we must
*       lock/unlock to prevent collisions.
*
*Entry:
*       wchar_t *string - place to put destination string
*ifdef _COUNT_
*       size_t count - max number of bytes to put in buffer
*endif
*       wchar_t *format - format string, describes format of data
*       va_list ap - varargs argument pointer
*
*Exit:
*       returns number of wide characters in string
*       returns -2 if the string has been truncated (only in _vsnprintf_helper)
*       returns -1 in other error cases
*
*Exceptions:
*
*******************************************************************************/

#ifndef _COUNT_

int __cdecl _vswprintf_l (
        wchar_t *string,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
#else

#ifndef _SWPRINTFS_ERROR_RETURN_FIX
/* Here we implement _vsnwprintf without the
return value bugfix */

int __cdecl _vsnwprintf_l (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
#else
int __cdecl _vswprintf_helper (
        WOUTPUTFN woutfn,
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
#endif        
#endif

{
        FILE str = { 0 };
        FILE *outfile = &str;
        int retval;

        _VALIDATE_RETURN( (format != NULL), EINVAL, -1);
        
#ifdef _COUNT_
        _VALIDATE_RETURN( (count == 0) || (string != NULL), EINVAL, -1 );
#else
        _VALIDATE_RETURN( (string != NULL), EINVAL, -1 );
#endif

        outfile->_flag = _IOWRT|_IOSTRG;
        outfile->_ptr = outfile->_base = (char *) string;
#ifndef _COUNT_
        outfile->_cnt = MAXSTR;
#else
        if(count>(INT_MAX/sizeof(wchar_t)))
        {
           /* old-style functions allow any large value to mean unbounded */
           outfile->_cnt = INT_MAX;
        }
        else
        {
            outfile->_cnt = (int)(count*sizeof(wchar_t));
        }
#endif

#ifndef _SWPRINTFS_ERROR_RETURN_FIX
        retval = _woutput_l(outfile, format, plocinfo, ap );
#else        
        retval = woutfn(outfile, format, plocinfo, ap );        
#endif        

        if(string==NULL)
        {
            return retval;
        }
        
#ifndef _SWPRINTFS_ERROR_RETURN_FIX
        _putc_nolock('\0',outfile);     /* no-lock version */
        _putc_nolock('\0',outfile);     /* 2nd byte for wide char version */

        return(retval);
#else
        if((retval >= 0) && (_putc_nolock('\0',outfile) != EOF) && (_putc_nolock('\0',outfile) != EOF))
            return(retval);
            
        string[count - 1] = 0;
        if (outfile->_cnt < 0)
        {
            /* the buffer was too small; we return -2 to indicate truncation */
            return -2;
        }
        return -1;
#endif        

}

#ifndef _COUNT_

int __cdecl _vswprintf (
        wchar_t *string,
        const wchar_t *format,
        va_list ap
        )
{
#pragma warning(push)
#pragma warning(disable:4996)
    return _vswprintf_l(string, format, NULL, ap);
#pragma warning(pop)
}        
#ifdef _WIN32_WCE
int __cdecl vswprintf (
        wchar_t *string,
        const wchar_t *format,
        va_list ap
        )
{
#pragma warning(push)
#pragma warning(disable:4996)
    return _vswprintf_l(string, format, NULL, ap);
#pragma warning(pop)
}  
#endif
int __cdecl __vswprintf_l (
        wchar_t *string,
        const wchar_t *format,
        _locale_t _Plocinfo,
        va_list ap
        )
{
#pragma warning(push)
#pragma warning(disable:4996)
    return _vswprintf_l(string, format, _Plocinfo, ap);
#pragma warning(pop)
}        

#else

#ifndef _SWPRINTFS_ERROR_RETURN_FIX
/* Here we implement _vsnwprintf without the
return value bugfix */

int __cdecl _vsnwprintf (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        va_list ap
        )
{
    #pragma warning(push)
    #pragma warning(disable:4996) // Disable deprecation warning since calling function is also deprecated
    return _vsnwprintf_l(string, count, format, NULL, ap);
    #pragma warning(pop)
}
#endif
#endif


#ifdef _SWPRINTFS_ERROR_RETURN_FIX
int __cdecl _vswprintf_c (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        va_list ap
        )
{
    int retval = _vswprintf_helper(_woutput_l, string, count, format, NULL, ap);
    return (retval < 0 ? -1 : retval);
}

int __cdecl _vswprintf_c_l (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    int retval = _vswprintf_helper(_woutput_l, string, count, format, plocinfo, ap);
    return (retval < 0 ? -1 : retval);
}

int __cdecl _vswprintf_s_l (
        wchar_t *string,
        size_t sizeInWords,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    int retvalue = -1;

    /* validation section */
    _VALIDATE_RETURN(format != NULL, EINVAL, -1);
    _VALIDATE_RETURN(string != NULL && sizeInWords > 0, EINVAL, -1);

    retvalue = _vswprintf_helper(_woutput_s_l, string, sizeInWords, format, plocinfo, ap);
    if (retvalue < 0)
    {
        string[0] = 0;
        _SECURECRT__FILL_STRING(string, sizeInWords, 1);
    }
    if (retvalue == -2)
    {
        _VALIDATE_RETURN(("Buffer too small", 0), ERANGE, -1);
    }
    if (retvalue >= 0)
    {
        _SECURECRT__FILL_STRING(string, sizeInWords, retvalue + 1);
    }

    return retvalue;
}

int __cdecl vswprintf_s (
        wchar_t *string,
        size_t sizeInWords,
        const wchar_t *format,
        va_list ap
        )
{
    return _vswprintf_s_l(string, sizeInWords, format, NULL, ap);
}

int __cdecl _vsnwprintf_s_l (
        wchar_t *string,
        size_t sizeInWords,
        size_t count,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    int retvalue = -1;
    errno_t save_errno = 0;

    /* validation section */
    _VALIDATE_RETURN(format != NULL, EINVAL, -1);
    if (count == 0 && string == NULL && sizeInWords == 0)
    {
        /* this case is allowed; nothing to do */
        return 0;
    }
    _VALIDATE_RETURN(string != NULL && sizeInWords > 0, EINVAL, -1);

    if (sizeInWords > count)
    {
        save_errno = errno;
        retvalue = _vswprintf_helper(_woutput_s_l, string, count + 1, format, plocinfo, ap);
        if (retvalue == -2)
        {
            /* the string has been truncated, return -1 */
            _SECURECRT__FILL_STRING(string, sizeInWords, count + 1);
            if (errno == ERANGE)
            {
                errno = save_errno;
            }
            return -1;
        }
    }
    else /* sizeInWords <= count */
    {
        save_errno = errno;
        retvalue = _vswprintf_helper(_woutput_s_l, string, sizeInWords, format, plocinfo, ap);
        string[sizeInWords - 1] = 0;
        /* we allow truncation if count == _TRUNCATE */
        if (retvalue == -2 && count == _TRUNCATE)
        {
            if (errno == ERANGE)
            {
                errno = save_errno;
            }
            return -1;
        }
    }

    if (retvalue < 0)
    {
        string[0] = 0;
        _SECURECRT__FILL_STRING(string, sizeInWords, 1);
        if (retvalue == -2)
        {
            _VALIDATE_RETURN(("Buffer too small", 0), ERANGE, -1);
        }
        return -1;
    }

    _SECURECRT__FILL_STRING(string, sizeInWords, retvalue + 1);

    return (retvalue < 0 ? -1 : retvalue);
}

int __cdecl _vsnwprintf_s (
        wchar_t *string,
        size_t sizeInWords,
        size_t count,
        const wchar_t *format,
        va_list ap
        )
{
    return _vsnwprintf_s_l(string, sizeInWords, count, format, NULL, ap);
}

int __cdecl _vswprintf_p (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        va_list ap
        )
{
    int retval = _vswprintf_helper(_woutput_p_l, string, count, format, NULL, ap);
    return (retval < 0 ? -1 : retval);
}

int __cdecl _vswprintf_p_l (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    int retval = _vswprintf_helper(_woutput_p_l, string, count, format, plocinfo, ap);
    return (retval < 0 ? -1 : retval);
}

#endif

/***
* _vscwprintf() - counts the number of character needed to print the formatted
* data
*
*Purpose:
*       Counts the number of characters in the fotmatted data.
*
*Entry:
*       wchar_t *format - format string, describes format of data
*       va_list ap - varargs argument pointer
*
*Exit:
*       returns number of characters needed to print formatted data.
*
*Exceptions:
*
*******************************************************************************/


#ifndef _COUNT_
int __cdecl _vscwprintf_helper (
        WOUTPUTFN outfn,
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
        FILE str = { 0 };
        FILE *outfile = &str;
        int retval;

        _VALIDATE_RETURN( (format != NULL), EINVAL, -1);

        outfile->_cnt = MAXSTR;
        outfile->_flag = _IOWRT|_IOSTRG;
        outfile->_ptr = outfile->_base = NULL;

        retval = outfn(outfile, format, plocinfo, ap);
        return(retval);
}

int __cdecl _vscwprintf (
        const wchar_t *format,
        va_list ap
        )
{
        return _vscwprintf_helper(_woutput_l, format, NULL, ap);
}

int __cdecl _vscwprintf_l (
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
        return _vscwprintf_helper(_woutput_l, format, plocinfo, ap);
}

int __cdecl _vscwprintf_p (
        const wchar_t *format,
        va_list ap
        )
{
        return _vscwprintf_helper(_woutput_p_l, format, NULL, ap);
}

int __cdecl _vscwprintf_p_l (
        const wchar_t *format,
        _locale_t plocinfo,
        va_list ap
        )
{
        return _vscwprintf_helper(_woutput_p_l, format, plocinfo, ap);
}

#endif
#endif  /* _POSIX_ */
