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
*mbtokata.c - Converts character to katakana.
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Converts a character from hiragana to katakana.
*
*Revision History:
*	11-19-92  KRS	Ported from 16-bit sources.
*	08-20-93  CFW   Change short params to int for 32-bit tree.
*	09-24-93  CFW	Removed #ifdef _KANJI
*	09-29-93  CFW	Return c unchanged if not Kanji code page.
*	10-06-93  GJF	Replaced _CRTAPI1 with __cdecl.
*	04-15-94  CFW	_ismbchira already tests for code page.
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
*unsigned short _mbctokata(c) - Converts character to katakana.
*
*Purpose:
*       If the character c is hiragana, convert to katakana.
*
*Entry:
*	unsigned int c - Character to convert.
*
*Exit:
*	Returns converted character.
*
*Exceptions:
*
*******************************************************************************/

unsigned int __cdecl _mbctokata_l(
        unsigned int c,
        _locale_t plocinfo
        )
{
	if (_ismbchira_l(c, plocinfo)) {
                c += 0xa1;
                if (c >= 0x837f)
                        c++;
        }
        return(c);
}
unsigned int (__cdecl _mbctokata)(
        unsigned int c
        )
{
    return _mbctokata_l(c, NULL);
}

#endif	/* _MBCS */
