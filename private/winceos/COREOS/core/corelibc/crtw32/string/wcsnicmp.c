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
*wcsnicmp.c - compare n chars of wide-character strings, ignoring case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _wcsnicmp() - Compares at most n characters of two wchar_t
*       strings, without regard to case.
*
*Revision History:
*       09-09-91  ETC   Created from strnicmp.c and wcsicmp.c.
*       12-09-91  ETC   Use C for neutral locale.
*       04-07-92  KRS   Updated and ripped out _INTL switches.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-14-93  CFW   Remove locale-sensitive portion.
*       09-07-93  GJF   Fixed bug introduced on 4-14 (return value not was
*                       not well-defined if count == 0).
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Remove _INTL switch.
*       10-25-94  GJF   Now works in non-C locales.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       10-11-95  BWT   Fix NTSUBSET
*       08-27-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       11-12-01  GB    Added support for new locale implementation.
*       10-06-03  AC    Added validation.
*       01-15-04  SJ    Added a check for count == 0 - VSW#219714
*       02-04-04  AJS   Allowed NULL pointers when count == 0 - VSWhidbey#225565
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>
#include <internal.h>

/***
*int _wcsnicmp(first, last, count) - compares count wchar_t of strings,
*       ignore case
*
*Purpose:
*       Compare the two strings for lexical order.  Stops the comparison
*       when the following occurs: (1) strings differ, (2) the end of the
*       strings is reached, or (3) count characters have been compared.
*       For the purposes of the comparison, upper case characters are
*       converted to lower case (wide-characters).
*
*Entry:
*       wchar_t *first, *last - strings to compare
*       size_t count - maximum number of characters to compare
*
*Exit:
*       Returns -1 if first < last
*       Returns 0 if first == last
*       Returns 1 if first > last
*       Returns _NLSCMPERROR is something went wrong
*       This range of return values may differ from other *cmp/*coll functions.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" int __cdecl _wcsnicmp_l (
        const wchar_t * first,
        const wchar_t * last,
        size_t count,
        _locale_t plocinfo
        )
{
    wchar_t f,l;
    int result = 0;

    if ( count )
    {
        /* validation section */
        _VALIDATE_RETURN(first != NULL, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(last != NULL, EINVAL, _NLSCMPERROR);

        _LocaleUpdate _loc_update(plocinfo);

        if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL )
        {
            do
            {
                f = __ascii_towlower(*first);
                l = __ascii_towlower(*last);
                first++;
                last++;
            }
            while ( (--count) && f && (f == l) );
        }
        else
        {
            do
            {
                f = _towlower_l( (unsigned short)(*first),_loc_update.GetLocaleT());
                l = _towlower_l( (unsigned short)(*last),_loc_update.GetLocaleT());
                first++;
                last++;
            }
            while ( (--count) && f && (f == l) );
        }

        result = (int)(f - l);
    }
    return result;
}

extern "C" int __cdecl _wcsnicmp (
        const wchar_t * first,
        const wchar_t * last,
        size_t count
        )
{
#ifndef _NTSUBSET_
    if (__locale_changed == 0)
    {
#endif  /* _NTSUBSET_ */

        wchar_t f,l;
        int result = 0;

        if(count)
        {
#ifndef _NTSUBSET_
            /* validation section */
            _VALIDATE_RETURN(first != NULL, EINVAL, _NLSCMPERROR);
            _VALIDATE_RETURN(last != NULL, EINVAL, _NLSCMPERROR);
#endif  /* _NTSUBSET_ */

            do {
                f = __ascii_towlower(*first);
                l = __ascii_towlower(*last);
                first++;
                last++;
            } while ( (--count) && f && (f == l) );
            
            result = (int)(f-l);
        }
        
        return result;

#ifndef _NTSUBSET_
    }
    else
    {
        return _wcsnicmp_l(first, last, count, NULL);
    }
#endif
}

#endif /* _POSIX_ */
