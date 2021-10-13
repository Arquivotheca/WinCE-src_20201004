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
*ismbcknj.c - contains the Kanji specific is* functions.
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Provide non-portable Kanji support for MBCS libs.
*
*Revision History:
*	11-19-92  KRS	Ported from 16-bit sources.
*	09-24-93  CFW	Removed #ifdef _KANJI
*	10-05-93  GJF	Replaced _CRTAPI1 with __cdecl.
*	10-22-93  CFW	Kanji-specific is*() functions return 0 outside Japan.
*
*******************************************************************************/

#ifdef _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbstring.h>
#include <mbctype.h>
#include <locale.h>
#include <setlocal.h>


/***
*int _ismbchira(c) - test character for hiragana (Japanese)
*
*Purpose:
*       Test if the character c is a hiragana character.
*
*Entry:
*	unsigned int c - character to test
*
*Exit:
*       returns TRUE if CP == KANJI and character is hiragana, else FALSE
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _ismbchira_l(
        unsigned int c,
        _locale_t plocinfo
        )
{
    _LocaleUpdate _loc_update(plocinfo);

    return(_loc_update.GetLocaleT()->mbcinfo->mbcodepage == _KANJI_CP && c >= 0x829f && c <= 0x82f1);
}

extern "C" int __cdecl _ismbchira(
        unsigned int c
        )
{
    return _ismbchira_l(c, NULL);
}


/***
*int _ismbckata(c) - test character for katakana (Japanese)
*
*Purpose:
*	Tests to see if the character c is a katakana character.
*
*Entry:
*       unsigned int c - character to test
*
*Exit:
*	Returns TRUE if CP == KANJI and c is a katakana character, else FALSE.
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _ismbckata_l (
        unsigned int c,
        _locale_t plocinfo
        )
{
    _LocaleUpdate _loc_update(plocinfo);

    return(_loc_update.GetLocaleT()->mbcinfo->mbcodepage == _KANJI_CP && c >= 0x8340 && c <= 0x8396 && c != 0x837f);
}
extern "C" int __cdecl _ismbckata(
        unsigned int c
        )
{
    return _ismbckata_l(c, NULL);
}


/*** 
*int _ismbcsymbol(c) - Tests if char is punctuation or symbol of Microsoft Kanji
*		   code.
*
*Purpose:
*	Returns non-zero if the character is kanji punctuation.
*
*Entry:
*       unsigned int c - character to be tested
*
*Exit:
*	Returns non-zero if CP == KANJI and the specified char is punctuation or symbol of
*		Microsoft Kanji code, else 0.
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _ismbcsymbol_l(
        unsigned int c,
        _locale_t plocinfo
        )
{
    _LocaleUpdate _loc_update(plocinfo);

    return(_loc_update.GetLocaleT()->mbcinfo->mbcodepage == _KANJI_CP && c >= 0x8141 && c <= 0x81ac && c != 0x817f);
}

extern "C" int (__cdecl _ismbcsymbol)(
        unsigned int c
        )
{
    return _ismbcsymbol_l(c, NULL);
}

#endif	/* _MBCS */
