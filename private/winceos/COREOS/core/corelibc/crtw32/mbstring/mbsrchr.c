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
*mbsrchr.c - Search for last occurence of character (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Search for last occurence of character (MBCS)
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       08-20-93  CFW   Change short params to int for 32-bit tree.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-09-94  CFW   Optimize for SBCS.
*       05-19-94  CFW   Enable non-Win32.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       04-17-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*       10-09-03  AC    Added validation.
*       02-01-07  JKS   Fix DevDiv bug #63968
                        Validation should always be performed to meet with MSDN
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
* _mbsrchr - Search for last occurence of character (MBCS)
*
*Purpose:
*       Find the last occurrence of the specified character in
*       the supplied string.  Handles MBCS chars/strings correctly.
*
*Entry:
*       unsigned char *str = string to search in
*       unsigned int c = character to search for
*
*Exit:
*       returns pointer to last occurrence of c in str
*       returns NULL if c not found
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" _CONST_RETURN unsigned char * __cdecl _mbsrchr_l(
        const unsigned char *str,
        unsigned int c,
        _locale_t plocinfo
        )
{
        char *r = NULL;
        unsigned int cc;
        _LocaleUpdate _loc_update(plocinfo);

        /* validation section */
        _VALIDATE_RETURN(str != NULL, EINVAL, 0);

        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return (_CONST_RETURN unsigned char *)strrchr((const char *)str, c);

        do {
            cc = *str;
            if ( _ismbblead_l(cc, _loc_update.GetLocaleT()) ) {
                if(*++str) {
                    if (c == ((cc<<8)|*str))
                        r = (char *)str - 1;
                }
                else if(!r)
                    /* return pointer to '\0' */
                    r = (char *)str;
            }
            else if (c == cc)
                r = (char *)str;
        }
        while (*str++);

        return((_CONST_RETURN unsigned char *)r);
}

extern "C" _CONST_RETURN unsigned char * (__cdecl _mbsrchr)(
        const unsigned char *str,
        unsigned int c
        )
{
    return _mbsrchr_l(str, c, NULL);
}

#endif  /* _MBCS */
