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
*wcsnset.c - set first n wide-characters to single wide-character
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _wcsnset() - sets at most the first n characters of a 
*	wchar_t string to a given character.
*
*Revision History:
*	09-09-91  ETC	Created from strnset.c.
*	04-07-92  KRS	Updated and ripped out _INTL switches.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>

/***
*wchar_t *_wcsnset(string, val, count) - set at most count characters to val
*
*Purpose:
*	Sets the first count characters of string the character value.
*	If the length of string is less than count, the length of
*	string is used in place of n (wide-characters).
*
*Entry:
*	wchar_t *string - string to set characters in
*	wchar_t val - character to fill with
*	size_t count - count of characters to fill
*
*Exit:
*	returns string, now filled with count copies of val.
*
*Exceptions:
*
*******************************************************************************/

wchar_t * __cdecl _wcsnset (
	wchar_t * string,
	wchar_t val,
	size_t count
	)
{
	wchar_t *start = string;

	while (count-- && *string)
		*string++ = (wchar_t)val;

	return(start);
}

#endif /* _POSIX_ */
