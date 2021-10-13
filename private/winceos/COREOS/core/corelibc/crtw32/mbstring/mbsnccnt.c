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
*mbsnccnt.c - Return char count of MBCS string
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Return char count of MBCS string
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
* _mbsnccnt - Return char count of MBCS string
*
*Purpose:
*       Returns the number of chars between the start of the supplied
*       string and the byte count supplied.  That is, this routine
*       indicates how many chars are in the first "bcnt" bytes
*       of the string.
*
*Entry:
*       const unsigned char *string = pointer to string
*       unsigned int bcnt = number of bytes to scan
*
*Exit:
*       Returns number of chars between string and bcnt.
*
*       If the end of the string is encountered before bcnt chars were
*       scanned, then the length of the string in chars is returned.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" size_t __cdecl _mbsnccnt_l(
        const unsigned char *string,
        size_t bcnt,
        _locale_t plocinfo
        )
{
        size_t n;
        _LocaleUpdate _loc_update(plocinfo);

        _VALIDATE_RETURN(string != NULL || bcnt == 0, EINVAL, 0);

        for (n = 0; (bcnt-- && *string); n++, string++) {
            if ( _ismbblead_l(*string, _loc_update.GetLocaleT()) ) {
                if ( (!bcnt--) || (*++string == '\0'))
                    break;
            }
        }

        return(n);
}
extern "C" size_t (__cdecl _mbsnccnt)(
        const unsigned char *string,
        size_t bcnt
        )
{
    return _mbsnccnt_l(string, bcnt, NULL);
}
#endif  /* _MBCS */
