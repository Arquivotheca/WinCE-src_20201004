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
*iswctype.c - support isw* wctype functions/macros for wide characters
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines iswctype - support isw* wctype functions/macros for
*       wide characters (esp. > 255).
*
*Revision History:
*       10-11-91  ETC   Created.
*       12-08-91  ETC   Updated api; check type masks.
*       04-06-92  KRS   Change to match ISO proposal.  Fix logic error.
*       08-07-92  GJF   _CALLTYPE4 (bogus usage) -> _CRTAPI1 (legit).
*       08-20-92  KRS   Activated NLS support.
*       08-22-92  SRW   Allow INTL definition to be conditional for building ntcrt.lib
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       09-03-92  GJF   Merged last 4 changes.
*       01-15-93  CFW   Put #ifdef _INTL around wint_t d def to avoid warnings
*       02-12-93  CFW   Return d not c, remove CTRL-Z.
*       02-17-93  CFW   Include locale.h.
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       05-05-93  CFW   Change name from is_wctype to iswctype as per ISO.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       06-26-93  CFW   Support is_wctype forever.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       09-27-93  GJF   Merged NT SDK and Cuda.
*       11-09-93  CFW   Add code page for __crtxxx().
*       12-01-93  GJF   Build is_wctype for Dolphin as well as NT.
*       02-07-94  CFW   POSIXify.
*       04-18-93  CFW   Pass lcid to _GetStringType.
*       04-01-96  BWT   POSIX work.
*       01-26-97  GJF   Deleted test which forced error for all wide chars >
*                       255 in the C locale.
*       08-24-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       09-06-00  GB    Always use pwctype for first 256 characters.
*       11-12-01  GB    Added support for new locale implementation.
*       04-25-05  AC    Fixed small problem in _iswctype_l()
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <locale.h>
#include <awint.h>
#include <ctype.h>
#include <mtdll.h>
#include <setlocal.h>

/*
 *  Use GetStringTypeW() API so check that character type masks agree between
 *  ctype.h and winnls.h
 */
#if !defined(_NTSUBSET_) && !defined(_POSIX_)
#if     _UPPER != C1_UPPER  || /*IFSTRIP=IGN*/ \
        _LOWER != C1_LOWER  || \
        _DIGIT != C1_DIGIT  || \
        _SPACE != C1_SPACE  || \
        _PUNCT != C1_PUNCT  || \
        _CONTROL != C1_CNTRL
#error Character type masks do not agree in ctype and winnls
#endif
#endif

/***
*iswctype - support isw* wctype functions/macros.
*
*Purpose:
*       This function is called by the isw* wctype functions/macros
*       (e.g. iswalpha()) when their argument is a wide character > 255.
*       It is also a standard ITSCJ (proposed) ISO routine and can be called
*       by the user, even for characters < 256.
*       Returns true or false depending on whether the argument satisfies
*       the character class property encoded by the mask.  Returns 0 if the
*       argument is WEOF.
*
*       NOTE: The isw* functions are neither locale nor codepage dependent.
*
*Entry:
*       wchar_t c    - the wide character whose type is to be tested
*       wchar_t mask - the mask used by the isw* functions/macros
*                       corresponding to each character class property
*
*Exit:
*       Returns non-zero if c is of the character class.
*       Returns 0 if c is not of the character class.
*
*Exceptions:
*       Returns 0 on any error.
*
*******************************************************************************/

extern "C" int __cdecl _iswctype_l (
        wint_t c,
        wctype_t mask,
        _locale_t plocinfo
        )
{
    return iswctype(c, mask);
}

extern "C" int __cdecl iswctype (
        wint_t c,
        wctype_t mask
        )
{
    wint_t d;

    if (c == WEOF)
    {
        return 0;
    }

    if (c < 256)
    {
        return (int)(_pwctype[c] & mask);
    }

#if     !defined(_NTSUBSET_) && !defined(_POSIX_)
    if (GetStringTypeW(
               CT_CTYPE1,
               (LPCWSTR)&c,
               1,
               &d
               ) == 0)
#endif
    d = 0;

    return (int)(d & mask);
}


/***
*is_wctype - support obsolete name
*
*Purpose:
*       Name changed from is_wctype to iswctype. is_wctype must be supported.
*
*Entry:
*       wchar_t c    - the wide character whose type is to be tested
*       wchar_t mask - the mask used by the isw* functions/macros
*                       corresponding to each character class property
*
*Exit:
*       Returns non-zero if c is of the character class.
*       Returns 0 if c is not of the character class.
*
*Exceptions:
*       Returns 0 on any error.
*
*******************************************************************************/
extern "C" int __cdecl is_wctype (
        wint_t c,
        wctype_t mask
        )
{
    return iswctype(c, mask);
}
