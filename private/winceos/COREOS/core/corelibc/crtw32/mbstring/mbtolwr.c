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
*mbtolwr.c - Convert character to lower case (MBCS).
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Convert character to lower case (MBCS).
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       08-20-93  CFW   Change short params to int for 32-bit tree.
*       09-29-93  CFW   Merge _KANJI and _MBCS_OS
*       10-06-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-12-94  CFW   Make function generic.
*       04-21-94  CFW   Return bad chars unchanged.
*       05-16-94  CFW   Use _mbbtolower/upper.
*       05-17-94  CFW   Enable non-Win32.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       09-26-97  BWT   Fix POSIX
*       04-21-98  GJF   Implemented multithread support based on threadmbcinfo
*                       structs
*       05-17-99  PML   Remove all Macintosh support.
*
*******************************************************************************/

#ifdef  _MBCS

#if     !defined(_POSIX_)
#include <awint.h>
#include <mtdll.h>
#endif  /* !_POSIX_ */

#include <cruntime.h>
#include <ctype.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <locale.h>
#include <setlocal.h>


/***
* _mbctolower - Convert character to lower case (MBCS)
*
*Purpose:
*       If the given character is upper case, convert it to lower case.
*       Handles MBCS characters correctly.
*
*       Note:  Use test against 0x00FF instead of _ISLEADBYTE
*       to ensure that we don't call SBCS routine with a two-byte
*       value.
*
*Entry:
*       unsigned int c = character to convert
*
*Exit:
*       Returns converted character
*
*Exceptions:
*
*******************************************************************************/

extern "C" unsigned int __cdecl _mbctolower_l (
        unsigned int c,
        _locale_t plocinfo
        )
{
        unsigned char val[2];
#if     !defined(_POSIX_)
        unsigned char ret[4];
#endif  /* !_POSIX_ */
        _LocaleUpdate _loc_update(plocinfo);

        if (c > 0x00FF)
        {
            val[0] = (c >> 8) & 0xFF;
            val[1] = c & 0xFF;

            if ( !_ismbblead_l(val[0], _loc_update.GetLocaleT()) )
                return c;

#if     !defined(_POSIX_)

            if ( __crtLCMapStringA(
                        _loc_update.GetLocaleT(),
                        _loc_update.GetLocaleT()->mbcinfo->mblocalename,
                        LCMAP_LOWERCASE,
                        (const char *)val,
                        2,
                        (char *)ret,
                        2,
                        _loc_update.GetLocaleT()->mbcinfo->mbcodepage,
                        TRUE ) == 0 )
                return c;

            c = ret[1];
            c += ret[0] << 8;

            return c;

#else /* !_POSIX_ */

            if (c >= _MBUPPERLOW1 && c <= _MBUPPERHIGH1)
                c += _MBCASEDIFF1;
            else if (c >= _MBUPPERLOW2 && c <= _MBUPPERHIGH2)
                c += _MBCASEDIFF2;

            return c;

#endif /* !_POSIX_ */

        }
        else
            return (unsigned int)_mbbtolower_l((int)c, _loc_update.GetLocaleT());
}

extern "C" unsigned int (__cdecl _mbctolower) (
        unsigned int c
        )
{
    return _mbctolower_l(c, NULL);
}

#endif  /* _MBCS */
