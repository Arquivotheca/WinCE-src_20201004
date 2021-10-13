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
*_mbslen.c - Return number of multibyte characters in a multibyte string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Return number of multibyte characters in a multibyte string
*       excluding the terminal null.  Locale-dependent.
*
*Revision History:
*       10-01-91  ETC   Created.
*       12-08-91  ETC   Add multithread lock.
*       12-18-92  CFW   Ported to Cuda tree, changed _CALLTYPE1 to _CRTAPI1.
*       04-29-93  CFW   Change to const char *s.
*       06-01-93  CFW   Test for bad MB chars.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       06-03-93  CFW   Change name to avoid conflict with mbstring function.
*                       Change return type to size_t.
*       08-19-93  CFW   Disallow skipping LB:NULL combos.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       10-22-93  CFW   Test for invalid MB chars using global preset flag.
*       01-14-94  SRW   if _NTSUBSET_ defined call Rtl functions
*       09-06-94  CFW   Remove _INTL switch.
*       12-21-94  CFW   Remove invalid MB chars NT 3.1 hack.
*       02-06-95  CFW   assert -> _ASSERTE.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       04-01-96  BWT   POSIX work.
*       06-21-96  GJF   Purged DLL_FOR_WIN32S. Polished format a bit.
*       07-16-96  SKS   Added missing call to _unlock_locale()
*       02-27-98  RKP   Added 64 bit support.
*       07-22-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       11-12-01  GB    Added support for new locale implementation.
*       10-08-03  AC    Added validation.
*
*******************************************************************************/

#if     defined(_NTSUBSET_) || defined(_POSIX_)
#include <ntrtl.h>
#include <nturtl.h>
#endif
#include <cruntime.h>
#include <stdlib.h>
#include <locale.h>
#include <dbgint.h>
#include <ctype.h>
#include <mbctype.h>
#include <internal.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*_mbstrlen - Return number of multibyte characters in a multibyte string
*
*Purpose:
*       Return number of multibyte characters in a multibyte string
*       excluding the terminal null.  Locale-dependent.
*
*Entry:
*       char *s = string
*
*Exit:
*       Returns the number of multibyte characters in the string, or
*       Returns (size_t)-1 if the string contains an invalid multibyte character.
*       Also, errno is set to EILSEQ.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

size_t __cdecl _mbstrlen_l(
        const char *s,
        _locale_t plocinfo
        )
{
    size_t n;
    _LocaleUpdate _loc_update(plocinfo);

    _ASSERTE (_loc_update.GetLocaleT()->locinfo->mb_cur_max == 1 || _loc_update.GetLocaleT()->locinfo->mb_cur_max == 2);

    if ( _loc_update.GetLocaleT()->locinfo->mb_cur_max == 1 )
        /* handle single byte character sets */
        return strlen(s);

#if     !defined(_NTSUBSET_) && !defined(_POSIX_)

    /* verify all valid MB chars */
    if ( MultiByteToWideChar( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                              MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
                              s, 
                              -1, 
                              NULL, 
                              0 ) == 0 ) 
    {
#ifndef _WCE_BOOTCRT
        /* bad MB char */
        errno = EILSEQ;
#endif
        return (size_t)-1;
    }

    /* count MB chars */
    for (n = 0; *s; n++, s++) {
        if ( _isleadbyte_l((unsigned char)*s, _loc_update.GetLocaleT()) )
        {
            if (*++s == '\0')
                break;
        }
    }

#else

    {
        char *s1 = (char *)s;


        while (RtlAnsiCharToUnicodeChar( &s1 ) != UNICODE_NULL)
            ;

        n = s1 - s;
    }

#endif

    return(n);
}

size_t __cdecl _mbstrlen(
        const char *s
        )
{
    if (__locale_changed == 0)
    {
        return strlen(s);
    }
    else
    {
        return _mbstrlen_l(s, NULL);
    }
}
