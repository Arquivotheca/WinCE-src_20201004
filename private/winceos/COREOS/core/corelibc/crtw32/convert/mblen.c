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
*mblen.c - length of multibyte character
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Return the number of bytes contained in a multibyte character.
*
*Revision History:
*       03-19-90  KRS   Module created.
*       12-20-90  KRS   Include ctype.h.
*       03-20-91  KRS   Ported from 16-bit tree.
*       12-09-91  ETC   Updated comments; move __mb_cur_max to nlsdata1.c;
*                       add multithread.
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       06-01-93  CFW   Re-write; verify valid MB char, proper error return,
*                       optimize, correct conversion bug.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-27-93  GJF   Merged NT SDK and Cuda versions.
*       10-22-93  CFW   Test for invalid MB chars using global preset flag.
*       01-14-94  SRW   if _NTSUBSET_ defined call Rtl functions
*       09-06-94  CFW   Remove _INTL switch.
*       12-21-94  CFW   Remove invalid MB chars NT 3.1 hack.
*       01-07-95  CFW   Mac merge cleanup.
*       02-06-95  CFW   assert -> _ASSERTE.
*       04-01-96  BWT   POSIX work.
*       06-25-96  GJF   Replaced defined(_WIN32) with !defined(_MAC). Also, 
*                       polished format a bit.
*       02-27-98  RKP   Added 64 bit support.
*       07-27-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       05-17-99  PML   Remove all Macintosh support.
*       11-12-01  GB    Added support for new locale implementation.
*
*******************************************************************************/

#if     defined(_NTSUBSET_) || defined(_POSIX_)
#include <ntrtl.h>
#include <nturtl.h>
#endif

#include <internal.h>
#include <locale.h>
#include <cruntime.h>
#include <stdlib.h>
#include <ctype.h>
#include <dbgint.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*int mblen() - length of multibyte character
*
*Purpose:
*       Return the number of bytes contained in a multibyte character.
*       [ANSI].
*
*Entry:
*       const char *s = pointer to multibyte character
*       size_t      n = maximum length of multibyte character to consider
*
*Exit:
*       If s = NULL, returns 0, indicating we use (only) state-independent
*       character encodings.
*
*       If s != NULL, returns:   0  (if *s = null char),
*                               -1  (if the next n or fewer bytes not valid 
*                                   mbc),
*                               number of bytes contained in multibyte char
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _mblen_l
        (
        const char * s,
        size_t n,
        _locale_t plocinfo
        )
{
    if ( !s || !(*s) || (n == 0) )
        /* indicate do not have state-dependent encodings,
           empty string length is 0 */
        return 0;

    _LocaleUpdate _loc_update(plocinfo);

    _ASSERTE (_loc_update.GetLocaleT()->locinfo->mb_cur_max == 1 || _loc_update.GetLocaleT()->locinfo->mb_cur_max == 2);

#if     !defined(_NTSUBSET_) && !defined(_POSIX_)

    if ( _isleadbyte_l((unsigned char)*s, _loc_update.GetLocaleT()) )
    {
        /* multi-byte char */

        /* verify valid MB char */
        if ( _loc_update.GetLocaleT()->locinfo->mb_cur_max <= 1 || 
             (int)n < _loc_update.GetLocaleT()->locinfo->mb_cur_max ||
             MultiByteToWideChar( _loc_update.GetLocaleT()->locinfo->lc_codepage, 
                                  MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
                                  s, 
                                  _loc_update.GetLocaleT()->locinfo->mb_cur_max, 
                                  NULL, 
                                  0 ) == 0 )
            /* bad MB char */
            return -1;
        else
            return _loc_update.GetLocaleT()->locinfo->mb_cur_max;
    }
    else {
        /* single byte char */

        /* verify valid SB char */
        if ( MultiByteToWideChar( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                                  MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
                                  s, 
                                  1, 
                                  NULL, 
                                  0 ) == 0 )
            return -1;
        return sizeof(char);
    }

#else   /* _NTSUBSET_ */

    {
        char *s1 = (char *)s;

        RtlAnsiCharToUnicodeChar( &s1 );
        return (int)(s1 - s);
    }

#endif  /* _NTSUBSET_ */
}

extern "C" int __cdecl mblen
        (
        const char * s,
        size_t n
        )
{
    if (__locale_changed == 0)
    {
        return _mblen_l(s, n, &__initiallocalestructinfo);
    }
    else
    {
        return _mblen_l(s, n, NULL);
    }
}
