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
*tojisjms.c:  Converts JIS to JMS code, and JMS to JIS code.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Convert JIS code into Microsoft Kanji code, and vice versa.
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       05-28-93  KRS   Ikura #27: Validate output is legal JIS.
*       08-20-93  CFW   Change short params to int for 32-bit tree.
*       09-24-93  CFW   Removed #ifdef _KANJI
*       09-29-93  CFW   Return c unchanged if not Kanji code page.
*       10-06-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-21-98  GJF   Implemented multithread support based on threadmbcinfo
*                       structs
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbstring.h>
#include <mbctype.h>
#include <mtdll.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>

/*** 
*unsigned int _mbcjistojms(c) - Converts JIS code to Microsoft Kanji Code.
*
*Purpose:
*       Convert JIS code to Microsoft Kanji code.
*
*Entry:
*       unsigned int c - JIS code to be converted. First byte is the upper
*                          8 bits, and second is the lower 8 bits.
*
*Exit:
*       Returns related Microsoft Kanji Code. First byte is the upper 8 bits
*       and second byte is the lower 8 bits.
*
*Exceptions:
*       If c is out of range, _mbcjistojms returns zero.
*
*******************************************************************************/

extern "C" unsigned int __cdecl _mbcjistojms_l(
        unsigned int c,
        _locale_t plocinfo
    )
{
        unsigned int h, l;
        _LocaleUpdate _loc_update(plocinfo);

        if (_loc_update.GetLocaleT()->mbcinfo->mbcodepage != _KANJI_CP)
            return (c);

        h = (c >> 8) & 0xff;
        l = c & 0xff;
        if (h < 0x21 || h > 0x7e || l < 0x21 || l > 0x7e)
        {
            errno = EILSEQ;
            return 0;
        }
        if (h & 0x01) {    /* first byte is odd */
            if (l <= 0x5f)
                l += 0x1f;
            else
                l += 0x20;
        }
        else
            l += 0x7e;

        h = ((h - 0x21) >> 1) + 0x81;
        if (h > 0x9f)
            h += 0x40;
        return (h << 8) | l;
}
extern "C" unsigned int (__cdecl _mbcjistojms)(
    unsigned int c
    )
{
    return _mbcjistojms_l(c, NULL);
}


/*** 
*unsigned int _mbcjmstojis(c) - Converts Microsoft Kanji code into JIS code.
*
*Purpose:
*       To convert Microsoft Kanji code into JIS code.
*
*Entry:
*       unsigned int c - Microsoft Kanji code to be converted. First byte is
*                          the upper 8 bits, and the second is the lower 8 bits.
*
*Exit:
*       Returns related JIS Code. First byte is the upper 8 bits and the second
*       byte is the lower 8 bits. If c is out of range, return zero.
*
*Exceptions:
*
*******************************************************************************/

extern "C" unsigned int __cdecl _mbcjmstojis_l(
        unsigned int c,
        _locale_t plocinfo
        )
{
        unsigned int    h, l;
        _LocaleUpdate _loc_update(plocinfo);

        if ( _loc_update.GetLocaleT()->mbcinfo->mbcodepage != _KANJI_CP )
            return (c);

        h = (c >> 8) & 0xff;
        l = c & 0xff;

        /* make sure input is valid shift-JIS */
        if ( (!(_ismbblead_l(h, _loc_update.GetLocaleT()))) || (!(_ismbbtrail_l(l, _loc_update.GetLocaleT()))) )
        {
            errno = EILSEQ;
            return 0;
        }

        h -= (h >= 0xa0) ? 0xc1 : 0x81;
        if(l >= 0x9f) {
            c = (h << 9) + 0x2200;
            c |= l - 0x7e;
        } else {
            c = (h << 9) + 0x2100;
            c |= l - ((l <= 0x7e) ? 0x1f : 0x20);
        }

        /* not all shift-JIS maps to JIS, so make sure output is valid */
        if ( (c>0x7E7E) || (c<0x2121) || ((c&0xFF)>0x7E) || ((c&0xFF)<0x21) )
        {
            errno = EILSEQ;
            return 0;
        }

        return c;
}
extern "C" unsigned int (__cdecl _mbcjmstojis)(
        unsigned int c
        )
{
    return _mbcjmstojis_l(c, NULL);
}

#endif  /* _MBCS */
