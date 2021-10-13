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
*sprintf.c - print formatted to string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines sprintf() and _snprintf() - print formatted data to string
*
*Revision History:
*       09-02-83  RN    initial version
*       04-13-87  JCR   added const to declaration
*       06-24-87  JCR   (1) Made declaration conform to ANSI prototype and use
*                       the va_ macros; (2) removed SS_NE_DS conditionals.
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
*       03-19-90  GJF   Made calling type _CALLTYPE2, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-03-90  GJF   New-style function declarator.
*       09-24-91  JCR   Added _snprintf()
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-05-94  SKS   Change "#ifdef" inside comments to "*ifdef" to avoid
*                       problems with CRTL source release process.
*       02-06-94  CFW   assert -> _ASSERTE.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       03-10-00  GB    Added support for knowing the length of formatted
*                       string by passing NULL for input string.
*       03-16-00  GB    Added _scprintf()
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-02-04  AJS   VSWhidbey#231427 - allow zero-length buffer.
*       02-13-04  SJ    VSW#242637 - removing _printf_p from the headers
*       03-18-04  SJ    The fns are back in the headers, don't need the #define.
*       04-08-04  AC    Added _snprintf_s and _vsnprintf_s
*                       VSW#263680
*       09-22-04  AGH   Add _scprintf_p; call _vscprintf[_p] from _scprintf[_p]
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
*int sprintf(string, format, ...) - print formatted data to string
*else
*int _snprintf(string, cnt, format, ...) - print formatted data to string
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
*ifdef _COUNT_
*       The _snprintf() flavor takes a count argument that is
*       the max number of bytes that should be written to the
*       user's buffer.
*endif
*
*       Multi-thread: (1) Since there is no stream, this routine must
*       never try to get the stream lock (i.e., there is no stream
*       lock either). (2) Also, since there is only one statically
*       allocated 'fake' iob, we must lock/unlock to prevent collisions.
*
*Entry:
*       char *string - pointer to place to put output
*ifdef _COUNT_
*       size_t count - max number of bytes to put in buffer
*endif
*       char *format - format string to control data format/number
*       of arguments followed by list of arguments, number and type
*       controlled by format string
*
*Exit:
*       returns number of characters printed
*
*Exceptions:
*
*******************************************************************************/

#ifndef _COUNT_

int __cdecl sprintf (
        char *string,
        const char *format,
        ...
        )
#else

#ifndef _SWPRINTFS_ERROR_RETURN_FIX

int __cdecl _snprintf (
        char *string,
        size_t count,
        const char *format,
        ...
        )
#else

int __cdecl _snprintf_c (
        char *string,
        size_t count,
        const char *format,
        ...
        )

#endif

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
            outfile->_cnt = (int)(count);
        }
#endif
        outfile->_flag = _IOWRT|_IOSTRG;
        outfile->_ptr = outfile->_base = string;

        retval = _output_l(outfile,format,NULL,arglist);

        if (string == NULL)
            return(retval);

#ifndef _SWPRINTFS_ERROR_RETURN_FIX
        _putc_nolock('\0',outfile); /* no-lock version */

        return(retval);
#else
        if((retval >= 0) && (_putc_nolock('\0',outfile) != EOF))
            return(retval);

        string[0] = 0;
        return -1;
#endif        
}

#ifndef _COUNT_

int __cdecl _sprintf_l (
        char *string,
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    va_start(arglist, plocinfo);

#pragma warning(push)
#pragma warning(disable:4996) // Disable deprecation warning since calling function is also deprecated
    return _vsprintf_l(string, format, plocinfo, arglist);
#pragma warning(pop)
}

#else
#ifndef _SWPRINTFS_ERROR_RETURN_FIX

int __cdecl _snprintf_l (
        char *string,
        size_t count,
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    va_start(arglist, plocinfo);

#pragma warning(push)
#pragma warning(disable:4996) // Disable deprecation warning since calling function is also deprecated
    return _vsnprintf_l(string, count, format, plocinfo, arglist);
#pragma warning(pop)
}
#else

int __cdecl _snprintf_c_l (
        char *string,
        size_t count,
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
    va_list arglist;
    va_start(arglist, plocinfo);

    return _vsnprintf_c_l(string, count, format, plocinfo, arglist);

}

#endif
#endif

#ifndef _COUNT_
int __cdecl sprintf_s (
        char *string,
        size_t sizeInBytes,        
        const char *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return _vsprintf_s_l(string, sizeInBytes, format, NULL, arglist);
}

int __cdecl _sprintf_s_l (
        char *string,
        size_t sizeInBytes,        
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return _vsprintf_s_l(string, sizeInBytes, format, plocinfo, arglist);
}

int __cdecl _snprintf_s (
        char *string,
        size_t sizeInBytes,        
        size_t count,        
        const char *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return _vsnprintf_s_l(string, sizeInBytes, count, format, NULL, arglist);
}

int __cdecl _snprintf_s_l (
        char *string,
        size_t sizeInBytes,        
        size_t count,        
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return _vsnprintf_s_l(string, sizeInBytes, count, format, plocinfo, arglist);
}

int __cdecl _sprintf_p (
        char *string,
        size_t count,        
        const char *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return _vsprintf_p_l(string, count, format, NULL, arglist);
}

int __cdecl _sprintf_p_l (
        char *string,
        size_t count,        
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return _vsprintf_p_l(string, count, format, plocinfo, arglist);
}

#endif

/***
* _scprintf() - counts the number of character needed to print the formatted
* data
*
*Purpose:
*       Counts the number of characters in the fotmatted data.
*
*Entry:
*       char *format - format string to control data format/number
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
int __cdecl _scprintf (
        const char *format,
        ...
        )
{
        va_list arglist;
    
        va_start(arglist, format);
    
        return _vscprintf(format, arglist);
}

int __cdecl _scprintf_p (
        const char *format,
        ...
        )
{
        va_list arglist;
    
        va_start(arglist, format);
    
        return _vscprintf_p(format, arglist);

}

int __cdecl _scprintf_l (
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
       va_list arglist;
       va_start(arglist, plocinfo);
       
       return _vscprintf_l(format, plocinfo, arglist);
}

int __cdecl _scprintf_p_l (
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
    
        va_start(arglist, plocinfo);
    
        return _vscprintf_p_l(format, plocinfo, arglist);

}
#endif
