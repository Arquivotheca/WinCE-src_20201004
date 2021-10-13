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
*ismbdgt.c - Test if character is a digit (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Test if character is a digit (MBCS)
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       09-29-93  CFW   Merge _KANJI and _MBCS_OS
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-12-94  CFW   Make function generic.
*       04-29-94  CFW   Place c in char array.
*       05-17-94  CFW   Enable non-Win32.
*       03-16-97  RDK   Added error flag to __crtGetStringTypeA.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       09-26-97  BWT   Fix POSIX
*       03-31-98  GJF   Implemented multithread support based on threadmbcinfo
*                       structs
*       05-17-99  PML   Remove all Macintosh support.
*       06-03-00  PML   Don't get mbcinfo if not DBCS (vs7#116057).
*
*******************************************************************************/

#ifdef  _MBCS

#if     !defined(_POSIX_)
#include <windows.h>
#include <awint.h>
#endif  /* !_POSIX */

#include <cruntime.h>
#include <ctype.h>
#include <mbctype.h>
#include <mbstring.h>
#include <locale.h>
#include <mtdll.h>
#include <mbdata.h>
#include <setlocal.h>


/***
* _ismbcdigit - Test if character is a digit (MBCS)
*
*Purpose:
*       Tests the character to see if it is a digit.
*       Handles MBCS chars correctly.
*
*       Note:  Use test against 0x00FF instead of _ISLEADBYTE
*       to ensure that we don't call SBCS routine with a two-byte
*       value.
*
*Entry:
*       unsigned int *c = character to test
*
*Exit:
*       Returns TRUE if character is a digit, else FALSE
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _ismbcdigit_l(
        unsigned int c,
        _locale_t plocinfo
        )
{
    _LocaleUpdate _loc_update(plocinfo);

    if (c > 0x00FF)
    {

#if !defined(_POSIX_)

        char buf[2];
        unsigned short ctype[2] = {0};


        buf[0] = (c >> 8) & 0xFF;
        buf[1] = c & 0xFF;

        /* return FALSE if not in supported MB code page */
        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return 0;

        /*
         * Since 'c' could be two one-byte MB chars, we need room in the
         * ctype return array to handle this. In this case, the
         * second word in the return array will be non-zero.
         */

        if ( __crtGetStringTypeA(
                    _loc_update.GetLocaleT(),
                    CT_CTYPE1,
                    buf,
                    2,
                    ctype,
                    _loc_update.GetLocaleT()->mbcinfo->mbcodepage,
                    TRUE) == 0)
            return 0;

        /* ensure single MB character and test for type */
        return (ctype[1] == 0 && ctype[0] & (_DIGIT));

#else   /* !_POSIX */

        return ( (c >= _MBDIGITLOW) && (c <= _MBDIGITHIGH) ) ;

#endif  /* !_POSIX */

    } else
    {
        return _isdigit_l(c, _loc_update.GetLocaleT());
    }
}

extern "C" int (__cdecl _ismbcdigit)(
        unsigned int c
        )
{
       return _ismbcdigit_l(c, NULL);
}

#endif  /* _MBCS */
