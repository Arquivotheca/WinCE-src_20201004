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
*ismbbyte.c - Function versions of MBCS ctype macros
*
*	Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*	This files provides function versions of the character
*	classification a*d conversion macros in mbctype.h.
*
*Revision History:
*	11-19-92  KRS	Ported from 16-bit assembler sources.
*	09-08-93  CFW   Remove _KANJI test.
*	09-29-93  CFW	Change _ismbbkana, add _ismbbkprint.
*	10-05-93  GJF	Replaced _CRTAPI1, _CRTAPI3 with __cdecl.
*	04-08-94  CFW   Change to ismbbyte.
*	09-14-94  SKS	Add ifstrip directive comment
*	02-11-95  CFW	Remove _fastcall.
*	05-28-02  GB    replaced _ctype with _pctype which makes more sense.
*	03-24-04  MSL   Appropriately use passed in locale
*                       VSW#257218
*	05-05-04  GB    Fixed the _ismbbkana_l.
*
*******************************************************************************/

#ifdef _MBCS

#include <cruntime.h>
#include <ctype.h>
#include <mtdll.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <locale.h>
#include <setlocal.h>

/* defined in mbctype.h
; Define masks

; set bit masks for the possible kanji character types
; (all MBCS bit masks start with "_M")

_MS		equ	01h	; MBCS non-ascii single byte char
_MP		equ	02h	; MBCS punct
_M1		equ	04h	; MBCS 1st (lead) byte
_M2		equ	08h	; MBCS 2nd byte

*/

/* defined in ctype.h
; set bit masks for the possible character types

_UPPER		equ	01h	; upper case letter
_LOWER		equ	02h	; lower case letter
_DIGIT		equ	04h	; digit[0-9]
_SPACE		equ	08h	; tab, carriage return, newline,
				; vertical tab or form feed
_PUNCT		equ	10h	; punctuation character
_CONTROL	equ	20h	; control character
_BLANK		equ	40h	; space char
_HEX		equ	80h	; hexadecimal digit

*/

/* defined in ctype.h, mbdata.h
	extrn	__mbctype:byte		; MBCS ctype table
	extrn	__ctype_:byte		; ANSI/ASCII ctype table
*/


/***
* ismbbyte - Function versions of mbctype macros
*
*Purpose:
*
*Entry:
*	int = character to be tested
*Exit:
*	ax = non-zero = character is of the requested type
*	   =        0 = character is NOT of the requested type
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

int __cdecl x_ismbbtype_l(_locale_t plocinfo, unsigned int, int, int);


/* ismbbk functions */

extern "C" int (__cdecl _ismbbkalnum_l) (unsigned int tst, _locale_t plocinfo)
{
        return x_ismbbtype_l(plocinfo,tst,0,_MS);
}

extern "C" int (__cdecl _ismbbkalnum) (unsigned int tst)
{
        return x_ismbbtype_l(NULL,tst,0,_MS);
}

extern "C" int (__cdecl _ismbbkprint_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,0,(_MS | _MP));
}

extern "C" int (__cdecl _ismbbkprint) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,0,(_MS | _MP));
}

extern "C" int (__cdecl _ismbbkpunct_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,0,_MP);
}

extern "C" int (__cdecl _ismbbkpunct) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,0,_MP);
}


/* ismbb functions */

extern "C" int (__cdecl _ismbbalnum_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,(_ALPHA | _DIGIT), _MS);
}

extern "C" int (__cdecl _ismbbalnum) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,(_ALPHA | _DIGIT), _MS);
}

extern "C" int (__cdecl _ismbbalpha_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,_ALPHA, _MS);
}

extern "C" int (__cdecl _ismbbalpha) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,_ALPHA, _MS);
}

extern "C" int (__cdecl _ismbbgraph_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,(_PUNCT | _ALPHA | _DIGIT),(_MS | _MP));
}

extern "C" int (__cdecl _ismbbgraph) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,(_PUNCT | _ALPHA | _DIGIT),(_MS | _MP));
}

extern "C" int (__cdecl _ismbbprint_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,(_BLANK | _PUNCT | _ALPHA | _DIGIT),(_MS | _MP));
}

extern "C" int (__cdecl _ismbbprint) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,(_BLANK | _PUNCT | _ALPHA | _DIGIT),(_MS | _MP));
}

extern "C" int (__cdecl _ismbbpunct_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,_PUNCT, _MP);
}

extern "C" int (__cdecl _ismbbpunct) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,_PUNCT, _MP);
}


/* lead and trail */

extern "C" int (__cdecl _ismbblead_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,0,_M1);
}

extern "C" int (__cdecl _ismbblead) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,0,_M1);
}

extern "C" int (__cdecl _ismbbtrail_l) (unsigned int tst, _locale_t plocinfo)
{
	return x_ismbbtype_l(plocinfo,tst,0,_M2);
}

extern "C" int (__cdecl _ismbbtrail) (unsigned int tst)
{
	return x_ismbbtype_l(NULL,tst,0,_M2);
}


/* 932 specific */

extern "C" int (__cdecl _ismbbkana_l) (unsigned int tst, _locale_t plocinfo)
{
    _LocaleUpdate _loc_update(plocinfo);
    if(_loc_update.GetLocaleT()->mbcinfo &&
       _loc_update.GetLocaleT()->mbcinfo->mbcodepage == _KANJI_CP)
    {
        return x_ismbbtype_l(plocinfo,tst,0,(_MS | _MP));
    }
    return FALSE;
}

extern "C" int (__cdecl _ismbbkana) (unsigned int tst)
{
    return _ismbbkana_l(tst, NULL);
}

/***
* Common code
*
*      cmask = mask for _ctype[] table
*      kmask = mask for _mbctype[] table
*
*******************************************************************************/

static int __cdecl x_ismbbtype_l (_locale_t plocinfo, unsigned int tst, int cmask, int kmask)
{
    _LocaleUpdate _loc_update(plocinfo);

    /*
     * get input character and make sure < 256 
     */
	tst = (unsigned int)(unsigned char)tst;

	return  ((*(_loc_update.GetLocaleT()->mbcinfo->mbctype+1+tst)) & kmask) ||
		((cmask) ? ((*(_loc_update.GetLocaleT()->locinfo->pctype+tst)) & cmask) : 0);
}

#endif	/* _MBCS */
