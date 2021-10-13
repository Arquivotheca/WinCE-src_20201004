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
*mbsnextc.c - Get the next character in an MBCS string.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       To return the value of the next character in an MBCS string.
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-28-98  GJF   No more _ISLEADBYTE macro.
*       05-09-02  MSL   Only moves 1 char if presented with leadbyte, EOS
*                       VS7 340544
*       10-02-03  AC    Added validation.
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <internal.h>
#include <stdlib.h>
#include <locale.h>
#include <setlocal.h>

/*** 
*_mbsnextc:  Returns the next character in a string.
*
*Purpose:
*       To return the value of the next character in an MBCS string.
*       Does not advance pointer to the next character.
*
*Entry:
*       unsigned char *s = string
*
*Exit:
*       unsigned int next = next character.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" unsigned int __cdecl _mbsnextc_l(
        const unsigned char *s,
        _locale_t plocinfo
        )
{
        unsigned int  next = 0;
        _LocaleUpdate _loc_update(plocinfo);

        /* validation section */
        _VALIDATE_RETURN(s != NULL, EINVAL, 0);

        /* don't skip forward 2 if the leadbyte is followed by EOS (dud string)
           also don't assert as we are too low-level
        */
        if ( _ismbblead_l(*s, _loc_update.GetLocaleT()) && s[1]!='\0')
            next = ((unsigned int) *s++) << 8;

        next += (unsigned int) *s;

        return(next);
}
extern "C" unsigned int (__cdecl _mbsnextc)(
        const unsigned char *s
        )
{
    return _mbsnextc_l(s, NULL);
}

#endif  /* _MBCS */
