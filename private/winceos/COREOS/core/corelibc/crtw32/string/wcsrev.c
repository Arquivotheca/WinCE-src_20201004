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
*wcsrev.c - reverse a wide-character string in place
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _wcsrev() - reverse a wchar_t string in place (not including
*	L'\0' character)
*
*Revision History:
*	09-09-91  ETC	Created from strrev.c.
*	04-07-92  KRS	Updated and ripped out _INTL switches.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>

/***
*wchar_t *_wcsrev(string) - reverse a wide-character string in place
*
*Purpose:
*	Reverses the order of characters in the string.  The terminating
*	null character remains in place (wide-characters).
*
*Entry:
*	wchar_t *string - string to reverse
*
*Exit:
*	returns string - now with reversed characters
*
*Exceptions:
*
*******************************************************************************/

wchar_t * __cdecl _wcsrev (
	wchar_t * string
	)
{
	wchar_t *start = string;
	wchar_t *left = string;
	wchar_t ch;

	while (*string++)		  /* find end of string */
		;
	string -= 2;

	while (left < string)
	{
		ch = *left;
		*left++ = *string;
		*string-- = ch;
	}

	return(start);
}

#endif /* _POSIX_ */
