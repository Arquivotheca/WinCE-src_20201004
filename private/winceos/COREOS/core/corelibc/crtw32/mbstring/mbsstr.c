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
* mbsstr.c - Search for one MBCS string inside another
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Search for one MBCS string inside another
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       10-06-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       05-09-94  CFW   Optimize for SBCS.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       04-20-98  GJF   Implemented multithread support based on threadmbcinfo
*                       structs
*       05-09-02  MSL   Fixed to deal correctly with leadbyte, EOS sequence
*                       VS7 340553
*       10-09-03  AC    Added validation.
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <stddef.h>
#include <string.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>

/***
* _mbsstr - Search for one MBCS string inside another
*
*Purpose:
*       Find the first occurrence of str2 in str1.
*
*Entry:
*       unsigned char *str1 = beginning of string
*       unsigned char *str2 = string to search for
*
*Exit:
*       Returns a pointer to the first occurrence of str2 in
*       str1, or NULL if str2 does not occur in str1
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" _CONST_RETURN unsigned char * __cdecl _mbsstr_l(
        const unsigned char *str1,
        const unsigned char *str2,
        _locale_t plocinfo
        )
{
        unsigned char *cp, *s1, *s2, *endp;
        _LocaleUpdate _loc_update(plocinfo);

        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return (unsigned char *)strstr((const char *)str1, (const char *)str2);

        /* validation section */
        _VALIDATE_RETURN(str2 != NULL, EINVAL, 0);
        if ( *str2 == '\0')
            return (unsigned char *)str1;
        _VALIDATE_RETURN(str1 != NULL, EINVAL, 0);

        cp = (unsigned char *) str1;
        endp = (unsigned char *) (str1 + (strlen((const char *)str1) - strlen((const char *)str2)));

        while (*cp && (cp <= endp))
        {
            s1 = cp;
            s2 = (unsigned char *) str2;

            /*
             * MBCS: ok to ++ since doing equality comparison.
             * [This depends on MBCS strings being "legal".]
             */
            while ( *s1 && *s2 && (*s1 == *s2) )
                s1++, s2++;

            if (!(*s2))
                return(cp);     /* success! */

            /*
             * bump pointer to next char
             */
            if ( _ismbblead_l(*(cp++), _loc_update.GetLocaleT()) )
            {
                /*  don't move forward if we have leadbyte, EOS
                    means dud string was passed in.
                    Don't assert - too low level
                */
                if(*cp!='\0')
                {
                    cp++;
                }
            }
        }

        return(NULL);

}

extern "C" _CONST_RETURN unsigned char * (__cdecl _mbsstr)(
        const unsigned char *str1,
        const unsigned char *str2
        )
{
    return _mbsstr_l(str1, str2, NULL);
}

#endif  /* _MBCS */
