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
*wcsset.c - sets all characters of wchar_t string to given character
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _wcsset() - sets all of the characters in a string (except
*	the L'\0') equal to a given character (wide-characters).
*
*Revision History:
*	09-09-91  ETC	Created from strset.c.
*	04-07-92  KRS	Updated and ripped out _INTL switches.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>

#if defined(_M_X64) || defined(_M_IX86) || defined (_M_ARM)
#pragma warning(disable:4163)
#pragma function(_wcsset)
#endif

/***
*wchar_t *_wcsset(string, val) - sets all of string to val (wide-characters)
*
*Purpose:
*	Sets all of wchar_t characters in string (except the terminating '/0'
*	character) equal to val (wide-characters).
*
*
*Entry:
*	wchar_t *string - string to modify
*	wchar_t val - value to fill string with
*
*Exit:
*	returns string -- now filled with val's
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

wchar_t * __cdecl _wcsset (
	wchar_t * string,
	wchar_t val
	)
{
	wchar_t *start = string;

	while (*string)
		*string++ = (wchar_t)val;

	return(start);
}

#endif /* _POSIX_ */
