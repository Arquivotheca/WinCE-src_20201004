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
*strncat.c - append n chars of string to new string
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines strncat() - appends n characters of string onto
*	end of other string
*
*Revision History:
*	05-31-89   JCR	C version created.
*	02-27-90   GJF	Fixed calling type, #include <cruntime.h>, fixed
*			copyright.
*	10-02-90   GJF	New-style function declarator.
*	09-02-93   GJF	Replaced _CALLTYPE1 with __cdecl.
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>

/***
*char *strncat(front, back, count) - append count chars of back onto front
*
*Purpose:
*	Appends at most count characters of the string back onto the
*	end of front, and ALWAYS terminates with a null character.
*	If count is greater than the length of back, the length of back
*	is used instead.  (Unlike strncpy, this routine does not pad out
*	to count characters).
*
*Entry:
*	char *front - string to append onto
*	char *back - string to append
*	unsigned count - count of max characters to append
*
*Exit:
*	returns a pointer to string appended onto (front).
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

char * __cdecl strncat (
	char * front,
	const char * back,
	size_t count
	)
{
	char *start = front;

	while (*front++)
		;
	front--;

	while (count--)
		if (!(*front++ = *back++))
			return(start);

	*front = '\0';
	return(start);
}
