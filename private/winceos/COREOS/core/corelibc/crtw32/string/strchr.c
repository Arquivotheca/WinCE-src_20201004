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
*strchr.c - search a string for a given character
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines strchr() - search a string for a character
*
*Revision History:
*	05-31-89   JCR	C version created.
*	02-27-90   GJF	Fixed calling type, #include <cruntime.h>, fixed
*			copyright.
*	08-14-90   SBM	Compiles cleanly with -W3, removed now redundant
*			#include <stddef.h>
*	10-01-90   GJF	New-style function declarator.
*	09-01-93   GJF	Replaced _CALLTYPE1 with __cdecl.
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>

/***
*char *strchr(string, c) - search a string for a character
*
*Purpose:
*	Searches a string for a given character, which may be the
*	null character '\0'.
*
*Entry:
*	char *string - string to search in
*	char c - character to search for
*
*Exit:
*	returns pointer to the first occurence of c in string
*	returns NULL if c does not occur in string
*
*Exceptions:
*
*******************************************************************************/

char * __cdecl strchr (
	const char * string,
	int ch
	)
{
	while (*string && *string != (char)ch)
		string++;

	if (*string == (char)ch)
		return((char *)string);
	return(NULL);
}
