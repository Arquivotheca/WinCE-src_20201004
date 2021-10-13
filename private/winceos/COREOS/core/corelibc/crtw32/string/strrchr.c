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
*strrchr.c - find last occurrence of character in string
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines strrchr() - find the last occurrence of a given character
*	in a string.
*
*Revision History:
*	02-27-90   GJF	Fixed calling type, #include <cruntime.h>, fixed
*			copyright.
*	08-14-90   SBM	Compiles cleanly with -W3, removed now redundant
*			#include <stddef.h>
*	10-02-90   GJF	New-style function declarator.
*	09-03-93   GJF	Replaced _CALLTYPE1 with __cdecl.
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>

/***
*char *strrchr(string, ch) - find last occurrence of ch in string
*
*Purpose:
*	Finds the last occurrence of ch in string.  The terminating
*	null character is used as part of the search.
*
*Entry:
*	char *string - string to search in
*	char ch - character to search for
*
*Exit:
*	returns a pointer to the last occurrence of ch in the given
*	string
*	returns NULL if ch does not occurr in the string
*
*Exceptions:
*
*******************************************************************************/

char * __cdecl strrchr (
	const char * string,
	int ch
	)
{
	char *start = (char *)string;

	while (*string++)			/* find end of string */
		;
						/* search towards front */
	while (--string != start && *string != (char)ch)
		;

	if (*string == (char)ch)		/* char found ? */
		return( (char *)string );

	return(NULL);
}
