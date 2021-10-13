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
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines vsprintf(), _vsnprintf() and _vsnprintf_s() - print formatted output to
*       a string, get the data from an argument ptr instead of explicit
*       arguments.
*
*Revision History:
*       09-02-83  RN    original sprintf
*       06-17-85  TC    rewrote to use new varargs macros, and to be vsprintf
*       04-13-87  JCR   added const to declaration
*       11-07-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-27-88  PHG   Merged DLL and normal versions
*       06-13-88  JCR   Fake _iob entry is now static so that other routines
*                       can assume _iob entries are in DGROUP.
*       08-25-88  GJF   Define MAXSTR to be INT_MAX (from LIMITS.H).
*       06-06-89  JCR   386 mthread support
*       08-18-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-16-90  GJF   Fixed copyright
*       03-20-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-25-90  SBM   Replaced <assertm.h> by <assert.h>, <varargs.h> by
*                       <stdarg.h>
*       10-03-90  GJF   New-style function declarator.
*       09-24-91  JCR   Added _vsnprintf()
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-05-94  SKS   Change "#ifdef" inside comments to "*ifdef" to avoid
*                       problems with CRTL source release process.
*       02-06-94  CFW   assert -> _ASSERTE.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       03-10-00  GB    Added support for knowing the length of formatted
*                       string by passing NULL for input string.
*       03-16-00  GB    Added _vscprintf()
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-02-04  AJS   VSWhidbey#231427 - allow zero-length buffer.
*       04-08-04  AC    Added _snprintf_s and _vsnprintf_s
*                       VSW#263680
*       04-08-04  AC    Fixed behavior of snprintf when string exceeds count
*                       VSW#283508
*       08-05-04  AC    The secure versions call invalid_parameter if there is
*                       not enough space in the buffer.
*                       Added truncation behavior for _vsnprintf_s.
*                       Added string padding in debug.
*       09-22-04  AGH   Added _vscprintf_p and _vscprintf_helper
*       11-16-04  AC    Do not set errno to ERANGE in the truncation case
*                       VSW#411191
*       12-13-04  AC    Do not set string[count] to 0 with no reason
*                       VSW#422301
*       01-21-05  MSL   Fix integer overflow issue with output sizes
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <internal.h>
#include <limits.h>
#include <mtdll.h>
#include <stddef.h>

#define MAXSTR INT_MAX


/***
*ifndef _COUNT_
*int vsprintf(string, format, ap) - print formatted data to string from arg ptr
*else
*int _vsnprintf(string, cnt, format, ap) - print formatted data to string from arg ptr
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
*       The _vsnprintf() flavor takes a count argument that is
*       the max number of bytes that should be written to the
*       user's buffer.
*endif
*
*       Multi-thread: (1) Since there is no stream, this routine must never try
*       to get the stream lock (i.e., there is no stream lock either).  (2)
*       Also, since there is only one staticly allocated 'fake' iob, we must
*       lock/unlock to prevent collisions.
*
*Entry:
*       char *string - place to put destination string
*ifdef _COUNT_
*       size_t count - max number of bytes to put in buffer
*endif
*       char *format - format string, describes format of data
*       va_list ap - varargs argument pointer
*
*Exit:
*       returns number of characters in string
*       returns -2 if the string has been truncated (only in _vsnprintf_helper)
*       returns -1 in other error cases
*
*Exceptions:
*
*******************************************************************************/

#ifndef _COUNT_

#ifdef _SWPRINTFS_ERROR_RETURN_FIX
#error "_COUNT_ must be defined if _SWPRINTFS_ERROR_RETURN_FIX is defined"
#endif

int __cdecl _vsprintf_l (
        char *string,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )

#else

#ifndef _SWPRINTFS_ERROR_RETURN_FIX

int __cdecl _vsnprintf_l (
        char *string,
        size_t count,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
#else
int __cdecl _vsnprintf_helper (
        OUTPUTFN outfn,
        char *string,
        size_t count,
        const char *format,
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

#ifndef _COUNT_
        outfile->_cnt = MAXSTR;
#else
        if(count>INT_MAX)
        {
            /* old-style functions allow any large value to mean unbounded */
            outfile->_cnt = INT_MAX;
        }
        else
        {
            outfile->_cnt = (int)count;
        }
#endif
        
        outfile->_flag = _IOWRT|_IOSTRG;
        outfile->_ptr = outfile->_base = string;

#ifndef _SWPRINTFS_ERROR_RETURN_FIX
        retval = _output_l(outfile, format, plocinfo, ap );
#else
        retval = outfn(outfile, format, plocinfo, ap );
#endif        

        if ( string==NULL)
            return(retval);            

#ifndef _SWPRINTFS_ERROR_RETURN_FIX

        _putc_nolock('\0',outfile);

        return(retval);
#else
        if((retval >= 0) && (_putc_nolock('\0',outfile) != EOF))
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
int __cdecl vsprintf(
        char *string,
        const char *format,
        va_list ap
        )
{
#pragma warning(push)
#pragma warning(disable:4996) // Disable deprecation warning since calling function is also deprecated
    return _vsprintf_l(string, format, NULL, ap);
#pragma warning(pop)
}

#else
#ifndef _SWPRINTFS_ERROR_RETURN_FIX
int __cdecl _vsnprintf (
        char *string,
        size_t count,
        const char *format,
        va_list ap
        )
{
#pragma warning(push)
#pragma warning(disable:4996) // Disable deprecation warning since calling function is also deprecated
    return _vsnprintf_l(string, count, format, NULL, ap);
#pragma warning(pop)
}
#endif
#endif

/* _SWPRINTFS_ERROR_RETURN_FIX implies _COUNT_ */
#ifdef _SWPRINTFS_ERROR_RETURN_FIX

int __cdecl _vsnprintf_c (
        char *string,
        size_t count,
        const char *format,
        va_list ap
        )
{
    int retval = _vsnprintf_helper(_output_l, string, count, format, NULL, ap);
    return (retval < 0 ? -1 : retval);
}

int __cdecl _vsnprintf_c_l (
        char *string,
        size_t count,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    int retval = _vsnprintf_helper(_output_l, string, count, format, plocinfo, ap);
    return (retval < 0 ? -1 : retval);
}

int __cdecl _vsprintf_s_l (
        char *string,
        size_t sizeInBytes,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    int retvalue = -1;

    /* validation section */
    _VALIDATE_RETURN(format != NULL, EINVAL, -1);
    _VALIDATE_RETURN(string != NULL && sizeInBytes > 0, EINVAL, -1);

    retvalue = _vsnprintf_helper(_output_s_l, string, sizeInBytes, format, plocinfo, ap);
    if (retvalue < 0)
    {
        string[0] = 0;
        _SECURECRT__FILL_STRING(string, sizeInBytes, 1);
    }
    if (retvalue == -2)
    {
        _VALIDATE_RETURN(("Buffer too small", 0), ERANGE, -1);
    }
    if (retvalue >= 0)
    {
        _SECURECRT__FILL_STRING(string, sizeInBytes, retvalue + 1);
    }

    return retvalue;
}

int __cdecl vsprintf_s (
        char *string,
        size_t sizeInBytes,
        const char *format,
        va_list ap
        )
{
    return _vsprintf_s_l(string, sizeInBytes, format, NULL, ap);
}

int __cdecl _vsnprintf_s_l (
        char *string,
        size_t sizeInBytes,
        size_t count,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    int retvalue = -1;
    errno_t save_errno = 0;

    /* validation section */
    _VALIDATE_RETURN(format != NULL, EINVAL, -1);
    if (count == 0 && string == NULL && sizeInBytes == 0)
    {
        /* this case is allowed; nothing to do */
        return 0;
    }
    _VALIDATE_RETURN(string != NULL && sizeInBytes > 0, EINVAL, -1);

    if (sizeInBytes > count)
    {
        save_errno = errno;
        retvalue = _vsnprintf_helper(_output_s_l, string, count + 1, format, plocinfo, ap);
        if (retvalue == -2)
        {
            /* the string has been truncated, return -1 */
            _SECURECRT__FILL_STRING(string, sizeInBytes, count + 1);
            if (errno == ERANGE)
            {
                errno = save_errno;
            }
            return -1;
        }
    }
    else /* sizeInBytes <= count */
    {
        save_errno = errno;
        retvalue = _vsnprintf_helper(_output_s_l, string, sizeInBytes, format, plocinfo, ap);
        string[sizeInBytes - 1] = 0;
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
        _SECURECRT__FILL_STRING(string, sizeInBytes, 1);
        if (retvalue == -2)
        {
            _VALIDATE_RETURN(("Buffer too small", 0), ERANGE, -1);
        }
        return -1;
    }

    _SECURECRT__FILL_STRING(string, sizeInBytes, retvalue + 1);

    return (retvalue < 0 ? -1 : retvalue);
}

int __cdecl _vsnprintf_s (
        char *string,
        size_t sizeInBytes,
        size_t count,
        const char *format,
        va_list ap
        )
{
    return _vsnprintf_s_l(string, sizeInBytes, count, format, NULL, ap);
}

int __cdecl _vsprintf_p (
        char *string,
        size_t count,
        const char *format,
        va_list ap
        )
{
    int retval = _vsnprintf_helper(_output_p_l, string, count, format, NULL, ap);
    return (retval < 0 ? -1 : retval);
}

int __cdecl _vsprintf_p_l (
        char *string,
        size_t count,
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
    int retval = _vsnprintf_helper(_output_p_l, string, count, format, plocinfo, ap);
    return (retval < 0 ? -1 : retval);
}

#endif

/***
* _vscprintf() - counts the number of character needed to print the formatted
* data
*
*Purpose:
*       Counts the number of characters in the fotmatted data.
*
*Entry:
*       char *format - format string, describes format of data
*       va_list ap - varargs argument pointer
*
*Exit:
*       returns number of characters needed to print formatted data.
*
*Exceptions:
*
*******************************************************************************/

#ifndef _COUNT_

int __cdecl _vscprintf_helper (
        OUTPUTFN outfn,
        const char *format,
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

int __cdecl _vscprintf (
        const char *format,
        va_list ap
        )
{
        return _vscprintf_helper(_output_l, format, NULL, ap);
}

int __cdecl _vscprintf_l (
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
        return _vscprintf_helper(_output_l, format, plocinfo, ap);
}

int __cdecl _vscprintf_p (
        const char *format,
        va_list ap
        )
{
        return _vscprintf_helper(_output_p_l, format, NULL, ap);
}

int __cdecl _vscprintf_p_l (
        const char *format,
        _locale_t plocinfo,
        va_list ap
        )
{
        return _vscprintf_helper(_output_p_l, format, plocinfo, ap);
}
#endif
