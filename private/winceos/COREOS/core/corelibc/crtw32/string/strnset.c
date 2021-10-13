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
*strnset.c - set first n characters to single character
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _strnset() - sets at most the first n characters of a string
*	to a given character.
*
*Revision History:
*	02-27-90   GJF	Fixed calling type, #include <cruntime.h>, fixed
*			copyright.
*	08-14-90   SBM	Compiles cleanly with -W3
*	10-02-90   GJF	New-style function declarator.
*	01-18-91   GJF	ANSI naming.
*	09-03-93   GJF	Replaced _CALLTYPE1 with __cdecl.
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>

/***
*char *_strnset(string, val, count) - set at most count characters to val
*
*Purpose:
*	Sets the first count characters of string the character value.
*	If the length of string is less than count, the length of
*	string is used in place of n.
*
*Entry:
*	char *string - string to set characters in
*	char val - character to fill with
*	unsigned count - count of characters to fill
*
*Exit:
*	returns string, now filled with count copies of val.
*
*Exceptions:
*
*******************************************************************************/

char * __cdecl _strnset (
	char * string,
	int val,
	size_t count
	)
{
	char *start = string;

	while (count-- && *string)
		*string++ = (char)val;

	return(start);
}
