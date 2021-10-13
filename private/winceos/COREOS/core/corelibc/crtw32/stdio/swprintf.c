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
*swprintf.c - print formatted to string
*
*       Copyright (c) 1985-2001, Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _swprintf(), _swprintf_c and _snwprintf() - print formatted data 
*       to string
*
*Revision History:
*       05-16-92  KRS   Created from sprintf.c
*       02-18-93  SRW   Make FILE a local and remove lock usage.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*       09-05-94  SKS   Change "#ifdef" inside comments to "*ifdef" to avoid
*                       problems with CRTL source release process.
*       02-06-94  CFW   assert -> _ASSERTE.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       03-16-00  GB    Added _scwprintf()
*       11-01-02  SJ    Fixed swprintf functions for
*                       conformance vsWhidbey#2409
*       08-21-03  SJ    VSWhidbey#46431 - swprintf error return fix
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-02-04  AJS   VSWhidbey#231427 - allow zero-length buffer.
*       02-13-04  SJ    VSW#242637 - removing _printf_p from the headers
*       03-18-04  SJ    The fns are back in the headers, don't need the #define.
*       04-08-04  AC    Added _snwprintf_s and _vsnwprintf_s
*                       VSW#263680
*       09-22-04  AGH   Added _scwprintf_p; call _vscwprintf[_p] from 
*                       _scwprintf[_p]
*       01-21-05  MSL   Fix integer overflow issue with output sizes
*       03-08-05  PAL   Add non-conforming __swprintf_l (VSW 452537).
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
*int _swprintf(string, format, ...) - print formatted data to string
*else
*ifndef _SWPRINTFS_ERROR_RETURN_FIX
*int _snwprintf(string, cnt, format, ...) - print formatted data to string
*else
*int _swprintf_c(string, cnt, format, ...) - print formatted data to string
*endif
*endif
*
*Purpose:
*       Prints formatted data to the using the format string to
*       format data and getting as many arguments as called for
*       Sets up a FILE so file i/o operations can be used, make
*       string look like a huge buffer to it, but _flsbuf will
*       refuse to flush it if it fills up.  Appends '\0' to make
*       it a true string. _output does the real work here
*
*       Allocate the 'fake' _iob[] entry statically instead of on
*       the stack so that other routines can assume that _iob[]
*       entries are in are in DGROUP and, thus, are near.
*       
*       We alias swprintf to _swprintf
*
*ifdef _COUNT_
*ifndef _SWPRINTFS_ERROR_RETURN_FIX
*       The _snwprintf() flavor takes a count argument that is
*       the max number of wide characters that should be written to the
*       user's buffer.
*       We don't expose this function directly in the headers.
*else
*       The _swprintf_c() flavor does the same thing as the _snwprintf
*       above, but, it also fixes a bug in the return value in the case
*       when there isn't enough space to write the null terminator
*       We don't fix this bug in _snwprintf because of backward 
*       compatibility. In new code, however, _snwprintf is #defined to
*       _swprintf_c so users get the bugfix.
*
*endif
*
*       Multi-thread: (1) Since there is no stream, this routine must
*       never try to get the stream lock (i.e., there is no stream
*       lock either). (2) Also, since there is only one statically
*       allocated 'fake' iob, we must lock/unlock to prevent collisions.
*
*Entry:
*       wchar_t *string - pointer to place to put output
*ifdef _COUNT_
*       size_t count - max number of wide characters to put in buffer
*endif
*       wchar_t *format - format string to control data format/number
*       of arguments followed by list of arguments, number and type
*       controlled by format string
*
*Exit:
*       returns number of wide characters printed
*
*Exceptions:
*
*******************************************************************************/

#ifndef _COUNT_

int __cdecl _swprintf (
        wchar_t *string,
        const wchar_t *format,
        ...
        )
#else

#ifndef _SWPRINTFS_ERROR_RETURN_FIX
/* Here we implement _snwprintf without the
return value bugfix */

int __cdecl _snwprintf (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        ...
        )
#else
int __cdecl _swprintf_c (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        ...
        )
#endif /* ifndef _SWPRINTFS_ERROR_RETURN_FIX */

#endif

{
        FILE str = { 0 };
        FILE *outfile = &str;
        va_list arglist;
        int retval;

        _VALIDATE_RETURN( (format != NULL), EINVAL, -1);

#ifdef _COUNT_
        _VALIDATE_RETURN( (count == 0) || (string != NULL), EINVAL, -1 );
#else
        _VALIDATE_RETURN( (string != NULL), EINVAL, -1 );
#endif
        va_start(arglist, format);
        
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

        retval = _woutput_l(outfile,format,NULL,arglist);
        
	if (string == NULL)
		return (retval);

#ifndef _SWPRINTFS_ERROR_RETURN_FIX

        _putc_nolock('\0',outfile); /* no-lock version */
        _putc_nolock('\0',outfile); /* 2nd null byte for wchar_t version */

        return(retval);
#else
        if((retval >= 0) && (_putc_nolock('\0',outfile) != EOF) && (_putc_nolock('\0',outfile) != EOF))
            return(retval);

        string[0] = 0;
        return -1;
#endif
}

#ifndef _COUNT_

int __cdecl __swprintf_l (
        wchar_t *string,
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    va_start(arglist, plocinfo);
#pragma warning(push)
#pragma warning(disable:4996) // Disable deprecation warning since calling function is also deprecated
    return __vswprintf_l(string, format, plocinfo, arglist);
#pragma warning(pop)
}

#else

#ifndef _SWPRINTFS_ERROR_RETURN_FIX
/* Here we implement _snwprintf without the
return value bugfix */

int __cdecl _snwprintf_l (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    va_start(arglist, plocinfo);
#pragma warning(push)
#pragma warning(disable:4996) // Disable deprecation warning since calling function is also deprecated
    return _vsnwprintf_l(string, count, format, plocinfo, arglist);
#pragma warning(pop)
}

#else
int __cdecl _swprintf_c_l (
        wchar_t *string,
        size_t count,
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    va_start(arglist, plocinfo);

    return _vswprintf_c_l(string, count, format, plocinfo, arglist);
}

#endif /* ifndef _SWPRINTFS_ERROR_RETURN_FIX */

#endif

#ifndef _COUNT_
int __cdecl swprintf_s (
        wchar_t *string,
        size_t sizeInWords,        
        const wchar_t *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vswprintf_s_l(string, sizeInWords, format, NULL, arglist);
}

int __cdecl _snwprintf_s (
        wchar_t *string,
        size_t sizeInWords,        
        size_t count,        
        const wchar_t *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vsnwprintf_s_l(string, sizeInWords, count, format, NULL, arglist);
}

int __cdecl _swprintf_p (
        wchar_t *string,
        size_t count,        
        const wchar_t *format,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, format);
    
    return _vswprintf_p_l(string, count, format, NULL, arglist);
}

int __cdecl _swprintf_s_l (
        wchar_t *string,
        size_t sizeInWords,        
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vswprintf_s_l(string, sizeInWords, format, plocinfo, arglist);
}

int __cdecl _snwprintf_s_l (
        wchar_t *string,
        size_t sizeInWords,        
        size_t count,        
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vsnwprintf_s_l(string, sizeInWords, count, format, plocinfo, arglist);
}

int __cdecl _swprintf_p_l (
        wchar_t *string,
        size_t count,        
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    
    va_start(arglist, plocinfo);
    
    return _vswprintf_p_l(string, count, format, plocinfo, arglist);
}

#endif

/***
* _scwprintf() - counts the number of character needed to print the formatted
* data
*
*Purpose:
*       Counts the number of characters in the fotmatted data.
*
*Entry:
*       wchar_t *format - format string to control data format/number
*       of arguments followed by list of arguments, number and type
*       controlled by format string
*
*Exit:
*       returns number of characters needed to print formatted data.
*
*Exceptions:
*
*******************************************************************************/

#ifndef _COUNT_
int __cdecl _scwprintf (
        const wchar_t *format,
        ...
        )
{
        va_list arglist;

        va_start(arglist, format);
        
        return _vscwprintf(format, arglist);
}

int __cdecl _scwprintf_p (
        const wchar_t *format,
        ...
        )
{
        va_list arglist;

        va_start(arglist, format);
        
        return _vscwprintf_p(format, arglist);
}

int __cdecl _scwprintf_l (
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    va_start(arglist, plocinfo);

    return _vscwprintf_l(format, plocinfo,arglist);
}

int __cdecl _scwprintf_p_l (
        const wchar_t *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;

        va_start(arglist, plocinfo);
        
        return _vscwprintf_p_l(format, plocinfo, arglist);
}

#endif
#endif /* _POSIX_ */
