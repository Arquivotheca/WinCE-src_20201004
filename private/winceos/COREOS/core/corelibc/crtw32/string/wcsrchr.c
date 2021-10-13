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
*wcsrchr.c - find last occurrence of wchar_t character in wide string
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines wcsrchr() - find the last occurrence of a given character
*	in a string (wide-characters).
*
*Revision History:
*	09-09-91  ETC	Created from strrchr.c.
*	04-07-92  KRS	Updated and ripped out _INTL switches.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>

/***
*wchar_t *wcsrchr(string, ch) - find last occurrence of ch in wide string
*
*Purpose:
*	Finds the last occurrence of ch in string.  The terminating
*	null character is used as part of the search (wide-characters).
*
*Entry:
*	wchar_t *string - string to search in
*	wchar_t ch - character to search for
*
*Exit:
*	returns a pointer to the last occurrence of ch in the given
*	string
*	returns NULL if ch does not occurr in the string
*
*Exceptions:
*
*******************************************************************************/

wchar_t * __cdecl wcsrchr (
	const wchar_t * string,
	wchar_t ch
	)
{
	wchar_t *start = (wchar_t *)string;

	while (*string++)			/* find end of string */
		;
						/* search towards front */
	while (--string != start && *string != (wchar_t)ch)
		;

	if (*string == (wchar_t)ch)		/* wchar_t found ? */
		return( (wchar_t *)string );

	return(NULL);
}

#endif /* _POSIX_ */
