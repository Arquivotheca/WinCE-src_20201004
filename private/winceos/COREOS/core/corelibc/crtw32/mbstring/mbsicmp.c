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
*mbsicmp.c - Case-insensitive string comparision routine (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Case-insensitive string comparision routine (MBCS)
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       09-29-93  CFW   Merge _KANJI and _MBCS_OS
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       10-12-93  CFW   Compare lower case, not upper.
*       04-12-94  CFW   Make function generic.
*       05-05-94  CFW   Work around NT/Chico bug: CompareString ignores
*                       control characters.
*       05-09-94  CFW   Optimize for SBCS, no re-scan if CompareString fixed.
*       05-12-94  CFW   Back to hard-coded, CompareString sort is backwards.
*       05-16-94  CFW   Use _mbbtolower/upper.
*       05-19-94  CFW   Enable non-Win32.
*       08-15-96  RDK   For Win32, use NLS call to make uppercase.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       09-26-97  BWT   Fix POSIX
*       04-07-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*       05-17-99  PML   Remove all Macintosh support.
*       10-02-03  AC    Added validation.
*
*******************************************************************************/

#ifdef  _MBCS

#include <awint.h>
#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbctype.h>
#include <string.h>
#include <mbstring.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>

/***
* _mbsicmp - Case-insensitive string comparision routine (MBCS)
*
*Purpose:
*       Compares two strings for lexical order without regard to case.
*       Strings are compared on a character basis, not a byte basis.
*
*Entry:
*       char *s1, *s2 = strings to compare
*
*Exit:
*       Returns <0 if s1 < s2
*       Returns  0 if s1 == s2
*       Returns >0 if s1 > s2
*       Returns _NLSCMPERROR if something went wrong
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" int __cdecl _mbsicmp_l(
        const unsigned char *s1,
        const unsigned char *s2,
        _locale_t plocinfo
        )
{
        unsigned short c1, c2;
        _LocaleUpdate _loc_update(plocinfo);
#if     !defined(_POSIX_)
        int    retval;
        unsigned char szResult[4];
#endif  /* !_POSIX_ */

        /* validation section */
        _VALIDATE_RETURN(s1 != NULL, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(s2 != NULL, EINVAL, _NLSCMPERROR);

        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return _stricmp_l((const char *)s1, (const char *)s2, _loc_update.GetLocaleT());

        for (;;)
        {
            c1 = *s1++;
            if ( _ismbblead_l(c1, _loc_update.GetLocaleT()) )
            {
                if (*s1 == '\0')
                    c1 = 0;
                else
                {
#if     !defined(_POSIX_)
                    retval = __crtLCMapStringA(
                            _loc_update.GetLocaleT(),
                            _loc_update.GetLocaleT()->mbcinfo->mblocalename,
                            LCMAP_UPPERCASE,
                            (LPCSTR)s1 - 1,
                            2,
                            (LPSTR)szResult,
                            2,
                            _loc_update.GetLocaleT()->mbcinfo->mbcodepage,
                            TRUE );

                    if (retval == 1)
                        c1 = szResult[0];
                    else if (retval == 2)
                        c1 = (szResult[0] << 8) + szResult[1];
                    else
                    {
                        errno = EINVAL;
                        return _NLSCMPERROR;
                    }
                    s1++;
#else   /* !_POSIX_ */
                    c1 = ((c1 << 8) | *s1++);
                    if (c1 >= _MBUPPERLOW1 && c1 <= _MBUPPERHIGH1)
                        c1 += _MBCASEDIFF1;
                    else if (c1 >= _MBUPPERLOW2 && c1 <= _MBUPPERHIGH2)
                        c1 += _MBCASEDIFF2;
#endif  /* !_POSIX_ */
                }
            }
            else
                c1 = _mbbtolower_l(c1, _loc_update.GetLocaleT());

            c2 = *s2++;
            if ( _ismbblead_l(c2, _loc_update.GetLocaleT()) )
            {
                if (*s2 == '\0')
                    c2 = 0;
                else
                {
#if     !defined(_POSIX_)
                    retval = __crtLCMapStringA(
                            _loc_update.GetLocaleT(),
                            _loc_update.GetLocaleT()->mbcinfo->mblocalename,
                            LCMAP_UPPERCASE,
                            (LPCSTR)s2 - 1,
                            2,
                            (LPSTR)szResult,
                            2,
                            _loc_update.GetLocaleT()->mbcinfo->mbcodepage,
                            TRUE );

                    if (retval == 1)
                        c2 = szResult[0];
                    else if (retval == 2)
                        c2 = (szResult[0] << 8) + szResult[1];
                    else
                    {
                        errno = EINVAL;
                        return _NLSCMPERROR;
                    }
                    s2++;
#else    /* !_POSIX_ */
                    c2 = ((c2 << 8) | *s2++);
                    if (c2 >= _MBUPPERLOW1 && c2 <= _MBUPPERHIGH1)
                        c2 += _MBCASEDIFF1;
                    else if (c2 >= _MBUPPERLOW2 && c2 <= _MBUPPERHIGH2)
                        c2 += _MBCASEDIFF2;
#endif  /* !_POSIX_ */
                }
            }
            else
                c2 = _mbbtolower_l(c2, _loc_update.GetLocaleT());

            if (c1 != c2)
                return( (c1 > c2) ? 1 : -1 );

            if (c1 == 0)
                return(0);
        }
}

extern "C" int (__cdecl _mbsicmp)(
        const unsigned char *s1,
        const unsigned char *s2
        )
{
    return _mbsicmp_l(s1, s2, NULL);
}
#endif  /* _MBCS */
