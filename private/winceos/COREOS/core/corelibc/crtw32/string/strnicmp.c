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
*strnicmp.c - compare n chars of strings, ignoring case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _strnicmp() - Compares at most n characters of two strings,
*       without regard to case.
*
*Revision History:
*       02-27-90  GJF   Fixed calling type, #include <cruntime.h>, fixed
*                       copyright.
*       10-02-90  GJF   New-style function declarator.
*       01-18-91  GJF   ANSI naming.
*       10-11-91  GJF   Update! Comparison of final bytes must use unsigned
*                       chars.
*       09-03-93  GJF   Replaced _CALLTYPE1 with __cdecl.
*       09-21-93  CFW   Avoid cast bug.
*       01-13-94  CFW   Fix Comments.
*       10-19-94  GJF   Sped up C locale. Also, made multi-thread safe.
*       12-29-94  CFW   Merge non-Win32.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       11-15-95  BWT   Fix _NTSUBSET_
*       08-11-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       09-08-98  GJF   Split out ASCII-only version.
*       05-17-99  PML   Remove all Macintosh support.
*       26-01-00  GB    Modified strnicmp for performance.
*       09-03-00  GB    Moved the performance code to toupper and tolower.
*                       restored the original file.
*       11-12-01  GB    Added support for new locale implementation.
*       10-06-03  AC    Added validation.
*       03-24-04  MSL   Fixed count==0 case
*                       VSW#257559
*       10-08-04  AGH   Added validations to _strnicmp
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <internal.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*int _strnicmp(first, last, count) - compares count char of strings, ignore case
*
*Purpose:
*       Compare the two strings for lexical order.  Stops the comparison
*       when the following occurs: (1) strings differ, (2) the end of the
*       strings is reached, or (3) count characters have been compared.
*       For the purposes of the comparison, upper case characters are
*       converted to lower case.
*
*Entry:
*       char *first, *last - strings to compare
*       size_t count - maximum number of characters to compare
*
*Exit:
*       returns <0 if first < last
*       returns 0 if first == last
*       returns >0 if first > last
*       Returns _NLSCMPERROR is something went wrong
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" int __cdecl _strnicmp_l (
        const char * dst,
        const char * src,
        size_t count,
        _locale_t plocinfo
        )
{
    int f,l;

    if ( count )
    {
        _LocaleUpdate _loc_update(plocinfo);

        /* validation section */
        _VALIDATE_RETURN(dst != NULL, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(src != NULL, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(count <= INT_MAX, EINVAL, _NLSCMPERROR);
    
        if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL )
        {
            return __ascii_strnicmp(dst, src, count);
        }
        else
        {
            do
            {
                f = _tolower_l( (unsigned char)(*(dst++)), _loc_update.GetLocaleT() );
                l = _tolower_l( (unsigned char)(*(src++)), _loc_update.GetLocaleT() );
            }
            while (--count && f && (f == l) );
        }
        return( f - l );
    }

    return( 0 );
}


#ifndef _M_IX86

extern "C" int __cdecl __ascii_strnicmp (
        const char * first,
        const char * last,
        size_t count
        )
{
    if(count)
    {
        int f=0;
        int l=0;
        
        do
        {

            if ( ((f = (unsigned char)(*(first++))) >= 'A') &&
                    (f <= 'Z') )
                f -= 'A' - 'a';

            if ( ((l = (unsigned char)(*(last++))) >= 'A') &&
                    (l <= 'Z') )
                l -= 'A' - 'a';

        }
        while ( --count && f && (f == l) );

        return ( f - l );
    }
    else
    {
        return 0;
    }
}

#endif

extern "C" int __cdecl _strnicmp (
        const char * dst,
        const char * src,
        size_t count
        )
{
#if     !defined(_NTSUBSET_)

    if (__locale_changed == 0)
    {
        /* validation section */
        _VALIDATE_RETURN(dst != NULL, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(src != NULL, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(count <= INT_MAX, EINVAL, _NLSCMPERROR);

        return __ascii_strnicmp(dst, src, count);
    }
    else
    {
        return _strnicmp_l(dst, src, count, NULL);
    }

#else
    return __ascii_strnicmp(dst, src, count);
#endif
}
