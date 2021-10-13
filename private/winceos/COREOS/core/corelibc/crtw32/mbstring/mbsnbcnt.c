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
*mbsnbcnt.c - Returns byte count of MBCS string
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Returns byte count of MBCS string
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-19-94  CFW   Enable non-Win32.
*       04-15-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
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
#include <locale.h>
#include <setlocal.h>

/***
* _mbsnbcnt - Returns byte count of MBCS string
*
*Purpose:
*       Returns the number of bytes between the start of the supplied
*       string and the char count supplied.  That is, this routine
*       indicates how many bytes are in the first "ccnt" characters
*       of the string.
*
*Entry:
*       unsigned char *string = pointer to string
*       unsigned int ccnt = number of characters to scan
*
*Exit:
*       Returns number of bytes between string and ccnt.
*
*       If the end of the string is encountered before ccnt chars were
*       scanned, then the length of the string in bytes is returned.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" size_t __cdecl _mbsnbcnt_l(
        const unsigned char *string,
        size_t ccnt,
        _locale_t plocinfo
        )
{
        unsigned char *p;
        _LocaleUpdate _loc_update(plocinfo);

        /* validation section */
        _VALIDATE_RETURN(string != NULL || ccnt == 0, EINVAL, 0);

        for (p = (unsigned char *)string; (ccnt-- && *p); p++) {
            if ( _ismbblead_l(*p, _loc_update.GetLocaleT()) ) {
                if (*++p == '\0') {
                    --p;
                    break;
                }
            }
        }

        return ((size_t) ((char *)p - (char *)string));
}

extern "C" size_t (__cdecl _mbsnbcnt)(
        const unsigned char *string,
        size_t ccnt
        )
{
        return _mbsnbcnt_l(string, ccnt, NULL);
}

#endif  /* _MBCS */
