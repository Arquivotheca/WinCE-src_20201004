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
*mbsnbcat.c - concatenate string2 onto string1, max length n bytes
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       defines mbsnbcat() - concatenate maximum of n bytes
*
*Revision History:
*       08-03-93  KRS   Ported from 16-bit sources.
*       08-20-93  CFW   Update _MBCS_OS support.
*       09-24-93  CFW   Merge _MBCS_OS and _KANJI.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-09-94  CFW   Optimize for SBCS.
*       05-19-94  CFW   Enable non-Win32.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       04-07-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*       10-14-03  AC    Added validation.
*       04-07-04  MSL   Fixed bad call to mbstype with empty strings
*                       VSW#274124
*       04-29-04  SJ    VSW#292764 - Removed redundant code causing mkss to fail
*       08-16-04  SJ    Reverting fix for VSW#292764 - the fix was incorrect.
*       03-23-05  MSL   Review comment cleanup
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <string.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>


/***
* _mbsnbcat - concatenate max cnt bytes onto dst
*
*Purpose:
*       Concatenates src onto dst, with a maximum of cnt bytes copied.
*       Handles 2-byte MBCS characters correctly.
*
*Entry:
*       unsigned char *dst - string to concatenate onto
*       unsigned char *src - string to concatenate from
*       int cnt - number of bytes to copy
*
*Exit:
*       returns dst, with src (at least part) concatenated on
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" unsigned char * __cdecl _mbsnbcat_l(
        unsigned char *dst,
        const unsigned char *src,
        size_t cnt,
        _locale_t plocinfo
        )
{
        unsigned char *start;

        if (!cnt)
                return(dst);

        /* validation section */
        _VALIDATE_RETURN(dst != NULL, EINVAL, NULL);
        _VALIDATE_RETURN(src != NULL, EINVAL, NULL);

        _LocaleUpdate _loc_update(plocinfo);

        _BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return (unsigned char *)strncat((char *)dst, (const char *)src, cnt);
        _END_SECURE_CRT_DEPRECATION_DISABLE

        start = dst;
        while (*dst++)
                ;
        --dst;          // dst now points to end of dst string

        /* if last char in string is a lead byte, back up pointer */
        if ( dst!=start && _mbsbtype_l(start, (int) ((dst - start) - 1), _loc_update.GetLocaleT()) == _MBC_LEAD )
        {
            --dst;
        }

        /* copy over the characters */

        while (cnt--) {

            if ( _ismbblead_l(*src, _loc_update.GetLocaleT()) ) {
                *dst++ = *src++;
                if (cnt == 0) {   /* write null if cnt exhausted */
                    dst[-1] = '\0';
                    break;
                }
                cnt--;
                if ((*dst++ = *src++)=='\0') { /* or if no trail byte */
                    dst[-2] = '\0';
                    break;
                }
            }
            else if ((*dst++ = *src++) == '\0')
                break;

        }

        if ( dst!=start && _mbsbtype_l(start, (int) ((dst - start) - 1), _loc_update.GetLocaleT()) == _MBC_LEAD )
        {
            dst[-1] = '\0';
        }
        else
        {
            *dst = '\0';
        }

        return(start);
}

extern "C" unsigned char * (__cdecl _mbsnbcat)(
        unsigned char *dst,
        const unsigned char *src,
        size_t cnt
        )
{
    _BEGIN_SECURE_CRT_DEPRECATION_DISABLE
    return _mbsnbcat_l(dst, src, cnt, NULL);
    _END_SECURE_CRT_DEPRECATION_DISABLE
}
#endif  /* _MBCS */
