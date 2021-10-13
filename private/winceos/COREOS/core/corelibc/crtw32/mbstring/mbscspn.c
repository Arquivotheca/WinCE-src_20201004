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
*mbscspn.c - Find first string char in charset (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Find first string char in charset (MBCS)
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
*       10-08-03  AC    Added validation.
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
#include <internal.h>
#include <locale.h>
#include <setlocal.h>

/***
*ifndef _RETURN_PTR
* _mbscspn - Find first string char in charset (MBCS)
*else
* _mbspbrk - Find first string char in charset, pointer return (MBCS)
*endif
*
*Purpose:
*       Returns maximum leading segment of string
*       which consists solely of characters NOT from charset.
*       Handles MBCS chars correctly.
*
*Entry:
*       char *string = string to search in
*       char *charset = set of characters to scan over
*
*Exit:
*
*ifndef _RETURN_PTR
*       Returns the index of the first char in string
*       that is in the set of characters specified by control.
*
*       Returns 0, if string begins with a character in charset.
*else
*       Returns pointer to first character in charset.
*
*       Returns NULL if string consists entirely of characters
*       not from charset.
*endif
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

#ifndef _RETURN_PTR

extern "C" size_t __cdecl _mbscspn_l(
        const unsigned char *string,
        const unsigned char *charset,
        _locale_t plocinfo
        )
#else

extern "C" const unsigned char * __cdecl _mbspbrk_l(
        const unsigned char *string,
        const unsigned char  *charset,
        _locale_t plocinfo
        )
#endif

{
        unsigned char *p, *q;
        _LocaleUpdate _loc_update(plocinfo);

        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
#ifndef _RETURN_PTR
            return strcspn((const char *)string, (const char *)charset);
#else
            return (const unsigned char *)strpbrk((const char *)string, (const char *)charset);
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
        for (q = (unsigned char *)string; *q ; q++) {

            /* loop through the charset */
            for (p = (unsigned char *)charset; *p ; p++) {

                if ( _ismbblead_l(*p, _loc_update.GetLocaleT()) ) {
                    if (((*p == *q) && (p[1] == q[1])) || p[1] == '\0')
                        break;
                    p++;
                }
                else
                    if (*p == *q)
                        break;
            }

            if (*p != '\0')         /* end of charset? */
                break;              /* no, match on this char */

            if ( _ismbblead_l(*q, _loc_update.GetLocaleT()) )
                if (*++q == '\0')
                    break;
        }

#ifndef _RETURN_PTR
        return((size_t) (q - string));  /* index */
#else
        return((*q) ? q : NULL);        /* pointer */
#endif

}

#ifndef _RETURN_PTR

extern "C" size_t (__cdecl _mbscspn)(
        const unsigned char *string,
        const unsigned char *charset
        )
#else

extern "C" const unsigned char * (__cdecl _mbspbrk)(
        const unsigned char *string,
        const unsigned char  *charset
        )
#endif

{
#ifndef _RETURN_PTR
        return _mbscspn_l(string, charset, NULL);
#else
        return _mbspbrk_l(string, charset, NULL);
#endif
}

#endif  /* _MBCS */
