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
*towupper.c - convert wide character to upper case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines towupper().
*
*Revision History:
*       10-11-91  ETC   Created.
*       12-10-91  ETC   Updated nlsapi; added multithread.
*       04-06-92  KRS   Make work without _INTL also.
*       01-19-93  CFW   Changed LCMapString to LCMapStringW.
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       06-11-93  CFW   Fix error handling bug.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       09-29-93  GJF   Merged NT SDK and Cuda versions.
*       11-09-93  CFW   Add code page for __crtxxx().
*       01-14-94  SRW   if _NTSUBSET_ defined call Rtl functions
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Remove _INTL switch.
*       10-25-94  GJF   Sped up for C locale. Added __towupper_lk. Also,
*                       cleaned up pre-processor conditionals.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       04-01-96  BWT   POSIX work.
*       06-25-96  GJF   Removed DLL_FOR_WIN32S and cleaned up the format a
*                       wee bit.
*       08-27-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       11-12-01  GB    Added support for new locale implementation.
*       10-08-02  EAN   Declared towupper and towlower to take and return
*                       wint_t instead of wchar_t
*
*******************************************************************************/

#if     defined(_NTSUBSET_) || defined(_POSIX_)
#include <ntrtl.h>
#include <nturtl.h>
#endif

#include <cruntime.h>
#include <stdio.h>
#include <locale.h>
#include <awint.h>
#include <ctype.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*wint_t _towupper_l(c, ptloci) - convert wide character to upper case
*
*Purpose:
*       Multi-thread function only! Non-locking version of towupper.
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

extern "C" wint_t __cdecl _towupper_l (
        wint_t c,
        _locale_t plocinfo
        )
{
    wint_t widechar;

    if (c == WEOF)
        return c;

    _LocaleUpdate _loc_update(plocinfo);

    if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL )
        return __ascii_towupper(c);

    /* if checking case of c does not require API call, do it */
    if ( c < 256 ) {
        if ( !iswlower(c) ) {
            return c;
        } else {
            return _loc_update.GetLocaleT()->locinfo->pcumap[c];
        }
    }

    /* convert wide char to uppercase */
    if ( 0 == __crtLCMapStringW( 
                _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE], 
                LCMAP_UPPERCASE,
                (LPCWSTR)&c, 
                1, 
                (LPWSTR)&widechar, 
                1 ) )
    {
        return c;
    }

    return widechar;

}

/***
*wint_t towupper(c) - convert wide character to upper case
*
*Purpose:
*       towupper() returns the uppercase equivalent of its argument
*
*Entry:
*       c - wint_t value of character to be converted
*
*Exit:
*       if c is a lower case letter, returns wint_t value of upper case
*       representation of c. otherwise, it returns c.
*
*Exceptions:
*
*******************************************************************************/

extern "C" wint_t __cdecl towupper (
        wint_t c
        )
{
#if    !defined(_NTSUBSET_) && !defined(_POSIX_)

    return _towupper_l(c, NULL);
#else   /* _NTSUBSET_/_POSIX_ */

    return (wint_t) RtlUpcaseUnicodeChar( (wchar_t) c );

#endif  /* _NTSUBSET_/_POSIX_ */
}
