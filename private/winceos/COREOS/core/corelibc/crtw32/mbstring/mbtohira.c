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
*mbtohira.c - Convert character from katakana to hiragana (Japanese).
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _jtohira() - convert character to hiragana.
*
*Revision History:
*	11-19-92  KRS	Ported from 16-bit sources.
*	08-20-93  CFW   Change short params to int for 32-bit tree.
*	09-24-93  CFW	Removed #ifdef _KANJI
*	09-29-93  CFW	Return c unchanged if not Kanji code page.
*	10-06-93  GJF	Replaced _CRTAPI1 with __cdecl.
*	04-15-94  CFW	_ismbckata already tests for code page.
*
*******************************************************************************/

#ifdef _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbstring.h>
#include <mbctype.h>
#include <stdlib.h>


/***
*unsigned int _mbctohira(c) - Converts character to hiragana.
*
*Purpose:
*	Converts the character c from katakana to hiragana, if possible.
*
*Entry:
*	unsigned int c - Character to convert.
*
*Exit:
*	Returns the converted character.
*
*Exceptions:
*
*******************************************************************************/

unsigned int __cdecl _mbctohira_l(
        unsigned int c,
        _locale_t plocinfo
        )
{
	if (_ismbckata_l(c, plocinfo) && c <= 0x8393) {
                if (c < 0x837f)
                        c -= 0xa1;
                else
                        c -= 0xa2;
        }
        return(c);
}

unsigned int (__cdecl _mbctohira)(
        unsigned int c
        )
{
    return _mbctohira_l(c, NULL);
}

#endif	/* _MBCS */
