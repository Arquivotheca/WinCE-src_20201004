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
*mbsspn.c - Search for init substring of chars from control string (MBCS).
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Search for init substring of chars from control string (MBCS).
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-09-94  CFW   Optimize for SBCS.
*       05-19-94  CFW   Enable non-Win32.
*       09-14-94  SKS   Clean up preprocessor commands inside comments
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       04-21-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*       10-02-03  AC    Added validation.
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <string.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <stddef.h>
#include <tchar.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>

/***
*ifndef _RETURN_PTR
* _mbsspn - Find first string char not in charset (MBCS)
*else
* _mbsspnp - Find first string char not in charset, return pointer (MBCS)
*endif
*
*Purpose:
*       Returns maximum leading segment of string consisting solely
*       of characters from charset.  Handles MBCS characters correctly.
*
*Entry:
*       unsigned char *string = string to search in
*       unsigned char *charset = set of characters to scan over
*
*Exit:
*
*ifndef _RETURN_PTR
*       Returns index of first char in string not in control.
*       Returns 0, if string begins with a character not in charset.
*else
*       Returns pointer to first character not in charset.
*       Returns NULL if string consists entirely of characters from charset.
*endif
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

#ifndef _RETURN_PTR
extern "C" size_t __cdecl _mbsspn_l
#else
extern "C" unsigned char * __cdecl _mbsspnp_l
#endif
        (
        const unsigned char *string,
        const unsigned char *charset,
        _locale_t plocinfo
        )
{
        unsigned char *p, *q;
        _LocaleUpdate _loc_update(plocinfo);

        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
#ifndef _RETURN_PTR
            return strspn((const char *)string, (const char *)charset);
#else
        {
            size_t retval;
            retval = strspn((const char *)string, (const char *)charset);
            return (unsigned char *)(*(string + retval) ? string + retval : NULL);
        }
#endif

        /* validation section */
#ifndef _RETURN_PTR
        _VALIDATE_RETURN(string != NULL, EINVAL, 0);
        _VALIDATE_RETURN(charset != NULL, EINVAL, 0);
#else
        _VALIDATE_RETURN(string != NULL, EINVAL, NULL);
        _VALIDATE_RETURN(charset != NULL, EINVAL, NULL);
#endif

        /* loop through the string to be inspected */
        for (q = (unsigned char *)string; *q; q++) {

            /* loop through the charset */
            for (p = (unsigned char *)charset; *p; p++) {
                if ( _ismbblead_l(*p, _loc_update.GetLocaleT()) ) {
                    if (((*p == *q) && (p[1] == q[1])) || p[1] == '\0')
                        break;
                    p++;
                }
                else
                    if (*p == *q)
                        break;
            }

            if (*p == '\0')         /* end of charset? */
                break;              /* yes, no match on this char */

            if ( _ismbblead_l(*q, _loc_update.GetLocaleT()) )
                if (*++q == '\0')
                    break;
        }

#ifndef _RETURN_PTR
        return((size_t) (q - string));          /* index */
#else
        return((*q) ? q : NULL);        /* pointer */
#endif

}

#ifndef _RETURN_PTR
extern "C" size_t (__cdecl _mbsspn)
#else
extern "C" unsigned char * (__cdecl _mbsspnp)
#endif
        (
        const unsigned char *string,
        const unsigned char *charset
        )
{
#ifndef _RETURN_PTR
        return _mbsspn_l(string, charset, NULL);
#else
        return _mbsspnp_l(string, charset, NULL);
#endif
}

#endif  /* _MBCS */
