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
* mbschr.c - Search MBCS string for character
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Search MBCS string for a character
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       05-12-93  KRS   Fix handling of c=='\0'.
*       08-20-93  CFW   Change param type to int, use new style param declarators.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-09-94  CFW   Optimize for SBCS.
*       05-19-94  CFW   Enable non-Win32.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       04-06-98  GJF   Revised multithread support based on threadmbcinfo
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
* _mbschr - Search MBCS string for character
*
*Purpose:
*       Search the given string for the specified character.
*       MBCS characters are handled correctly.
*
*Entry:
*       unsigned char *string = string to search
*       int c = character to search for
*
*Exit:
*       returns a pointer to the first occurence of the specified char
*       within the string.
*
*       returns NULL if the character is not found n the string.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/


extern "C" _CONST_RETURN unsigned char * __cdecl _mbschr_l(
        const unsigned char *string,
        unsigned int c,
        _locale_t plocinfo
        )
{
        unsigned short cc;
        _LocaleUpdate _loc_update(plocinfo);

        /* validation section */
        _VALIDATE_RETURN(string != NULL, EINVAL, NULL);

        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return (_CONST_RETURN unsigned char *)strchr((const char *)string, (int)c);

        for (; (cc = *string); string++)
        {
            if ( _ismbblead_l(cc, _loc_update.GetLocaleT()) )
            {                   
                if (*++string == '\0')
                    return NULL;        /* error */
                if ( c == (unsigned int)((cc << 8) | *string) ) /* DBCS match */
                    return (unsigned char *)(string - 1);
            }
            else if (c == (unsigned int)cc)
                break;  /* SBCS match */
        }

        if (c == (unsigned int)cc)      /* check for SBCS match--handles NUL char */
            return (unsigned char *)(string);

        return NULL;
}

extern "C" _CONST_RETURN unsigned char * (__cdecl _mbschr)(
        const unsigned char *string,
        unsigned int c
        )
{
    return _mbschr_l(string, c, NULL);
}

#endif  /* _MBCS */
