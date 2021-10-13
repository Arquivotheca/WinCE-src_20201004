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
*memicmp.c - compare memory, ignore case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _memicmp() - compare two blocks of memory for lexical
*       order.  Case is ignored in the comparison.
*
*Revision History:
*       05-31-89  JCR   C version created.
*       02-27-90  GJF   Fixed calling type, #include <cruntime.h>, fixed
*                       copyright. Also, fixed compiler warnings.
*       10-01-90  GJF   New-style function declarator. Also, rewrote expr. to
*                       avoid using cast as an lvalue.
*       01-17-91  GJF   ANSI naming.
*       10-11-91  GJF   Update! Comparison of final bytes must use unsigned
*                       chars.
*       09-01-93  GJF   Replaced _CALLTYPE1 with __cdecl.
*       10-18-94  GJF   Sped up, especially for C locale. Also, made multi-
*                       thread safe.
*       12-29-94  CFW   Merge non-Win32.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       11-15-95  BWT   Fix _NTSUBSET_
*       08-27-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       09-08-98  GJF   Split out ASCII-only version.
*       05-17-99  PML   Remove all Macintosh support.
*       10-27-99  PML   Win64 fix: unsigned int -> size_t
*       26-01-00  GB    Modified memicmp for performance.
*       09-03-00  GB    Moved the performance code to toupper and tolower.
*                       restored the original file.
*       10-02-03  AC    Added validation.
*       10-08-04  AGH   Added validations to _memicmp
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <mtdll.h>
#include <ctype.h>
#include <locale.h>
#include <internal.h>
#include <setlocal.h>

/***
*int _memicmp(first, last, count) - compare two blocks of memory, ignore case
*
*Purpose:
*       Compares count bytes of the two blocks of memory stored at first
*       and last.  The characters are converted to lowercase before
*       comparing (not permanently), so case is ignored in the search.
*
*Entry:
*       char *first, *last - memory buffers to compare
*       size_t count - maximum length to compare
*
*Exit:
*       Returns < 0 if first < last
*       Returns 0 if first == last
*       Returns > 0 if first > last
*       Returns _NLSCMPERROR is something went wrong
*
*Uses:
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" int __cdecl _memicmp_l (
        const void * first,
        const void * last,
        size_t count,
        _locale_t plocinfo
        )
{
#if     !defined(_NTSUBSET_)
    int f = 0, l = 0;
    const char *dst = (const char *)first, *src = (const char *)last;
    _LocaleUpdate _loc_update(plocinfo);

    /* validation section */
    _VALIDATE_RETURN(first != NULL || count == 0, EINVAL, _NLSCMPERROR);
    _VALIDATE_RETURN(last != NULL || count == 0, EINVAL, _NLSCMPERROR);

    if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL )
    {
        return __ascii_memicmp(first, last, count);
    }
    else 
    {
        while (count-- && f==l)
        {
            f = _tolower_l( (unsigned char)(*(dst++)), _loc_update.GetLocaleT() );
            l = _tolower_l( (unsigned char)(*(src++)), _loc_update.GetLocaleT() );
        }
    }
    return ( f - l );
#else
    return __ascii_memicmp(first, last, count);
#endif  /* !_NTSUBSET_ */
}


#ifndef _M_IX86

extern "C" int __cdecl __ascii_memicmp (
        const void * first,
        const void * last,
        size_t count
        )
{
    int f = 0;
    int l = 0;

    while ( count-- )
    {
        if ( (*(unsigned char *)first == *(unsigned char *)last) ||
             ((f = __ascii_tolower( *(unsigned char *)first )) ==
              (l = __ascii_tolower( *(unsigned char *)last ))) )
        {
                first = (char *)first + 1;
                last = (char *)last + 1;
        }
        else
            break;
    }
    return ( f - l );
}

#endif

extern "C" int __cdecl _memicmp (
        const void * first,
        const void * last,
        size_t count
        )
{
    if (__locale_changed == 0)
    {
        /* validation section */
        _VALIDATE_RETURN(first != NULL || count == 0, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(last != NULL || count == 0, EINVAL, _NLSCMPERROR);

        return __ascii_memicmp(first, last, count);
    }
    else
    {
        return _memicmp_l(first, last, count, NULL);
    }
}
