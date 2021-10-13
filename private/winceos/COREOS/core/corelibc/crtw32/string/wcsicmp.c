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
*wcsicmp.c - contains case-insensitive wide string comp routine _wcsicmp
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       contains _wcsicmp()
*
*Revision History:
*       09-09-91  ETC   Created from stricmp.c.
*       12-09-91  ETC   Use C for neutral locale.
*       04-07-92  KRS   Updated and ripped out _INTL switches.
*       08-19-92  KRS   Actived use of CompareStringW.
*       08-22-92  SRW   Allow INTL definition to be conditional for building ntcrt.lib
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       12-15-92  KRS   Added robustness to non-_INTL code.  Optimize.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-14-93  CFW   Remove locale-sensitive portion.
*       02-07-94  CFW   POSIXify.
*       10-25-94  GJF   Now works in non-C locales.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       10-11-95  BWT   Fix NTSUBSET
*       08-27-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       11-12-01  GB    Added support for new locale implementation.
*       10-02-03  AC    Added validation.
*       10-08-04  AGH   Added validations to _wcsicmp
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <internal.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*int _wcsicmp(dst, src) - compare wide-character strings, ignore case
*
*Purpose:
*       _wcsicmp perform a case-insensitive wchar_t string comparision.
*       _wcsicmp is independent of locale.
*
*Entry:
*       wchar_t *dst, *src - strings to compare
*
*Return:
*       Returns <0 if dst < src
*       Returns 0 if dst = src
*       Returns >0 if dst > src
*       Returns _NLSCMPERROR is something went wrong
*       This range of return values may differ from other *cmp/*coll functions.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" int __cdecl _wcsicmp_l (
        const wchar_t * dst,
        const wchar_t * src,
        _locale_t plocinfo
        )
{
    wchar_t f,l;
    _LocaleUpdate _loc_update(plocinfo);

    /* validation section */
    _VALIDATE_RETURN(dst != NULL, EINVAL, _NLSCMPERROR);
    _VALIDATE_RETURN(src != NULL, EINVAL, _NLSCMPERROR);

    if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL)
    { 
        do 
        {
            f = __ascii_towlower(*dst);
            l = __ascii_towlower(*src);
            dst++;
            src++;
        }
        while ( (f) && (f == l) );
    }
    else
    {
        do 
        {
            f = _towlower_l((unsigned short)*(dst++), _loc_update.GetLocaleT());
            l = _towlower_l((unsigned short)*(src++), _loc_update.GetLocaleT());
        }
        while ( (f) && (f == l) );
    }
    return (int)(f - l);
}

extern "C" int __cdecl _wcsicmp (
        const wchar_t * dst,
        const wchar_t * src
        )
{
#ifndef _NTSUBSET_
    if (__locale_changed == 0)
    {
#endif
        wchar_t f,l;

#ifndef _NTSUBSET_
        /* validation section */
        _VALIDATE_RETURN(dst != NULL, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(src != NULL, EINVAL, _NLSCMPERROR);
#endif

        do  {
            f = __ascii_towlower(*dst);
            l = __ascii_towlower(*src);
            dst++;
            src++;
        } while ( (f) && (f == l) );
        return (int)(f - l);
#ifndef _NTSUBSET_
    }
    else
    {
        return _wcsicmp_l(dst, src, NULL);
    }
#endif
}

#endif /* _POSIX_ */
