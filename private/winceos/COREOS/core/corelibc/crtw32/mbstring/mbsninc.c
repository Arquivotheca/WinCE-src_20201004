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
*mbsninc.c - Increment MBCS string pointer by specified char count.
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Increment MBCS string pointer by specified char count.
*
*Revision History:
*	11-19-92  KRS	Ported from 16-bit sources.
*	08-03-93  KRS	Fix return value logic.
*	10-05-93  GJF	Replaced _CRTAPI1 with __cdecl.
*
*******************************************************************************/

#ifdef _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbstring.h>
#include <stddef.h>

/*** 
*_mbsninc - Increment MBCS string pointer by specified char count.
*
*Purpose:
*	Increment the supplied string pointer by the specified number
*	of characters.	MBCS characters are handled correctly.
*
*Entry:
*	const unsigned char *string = pointer to string
*	unsigned int ccnt = number of char to advance the pointer
*
*Exit:
*	Returns pointer after advancing it.
*	Returns pointer to end of string if string is not ccnt chars long.
*	Returns NULL is supplied pointer is NULL.
*
*Exceptions:
*
*******************************************************************************/

unsigned char * __cdecl _mbsninc_l(
        const unsigned char *string,
        size_t ccnt,
        _locale_t plocinfo
        )
{
	if (string == NULL)
		return(NULL);

	return((char *)string + (unsigned int)_mbsnbcnt_l(string, ccnt, plocinfo));
}

unsigned char * (__cdecl _mbsninc)(
        const unsigned char *string,
        size_t ccnt
        )
{
    return _mbsninc_l(string, ccnt, NULL);
}

#endif	/* _MBCS */
