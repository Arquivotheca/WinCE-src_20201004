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
*strncpy.c - copy at most n characters of string
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines strncpy() - copy at most n characters of string
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

#if     defined(_M_ARM)
#pragma function(strncpy)
#endif

/***
*char *strncpy(dest, source, count) - copy at most n characters
*
*Purpose:
*	Copies count characters from the source string to the
*	destination.  If count is less than the length of source,
*	NO NULL CHARACTER is put onto the end of the copied string.
*	If count is greater than the length of sources, dest is padded
*	with null characters to length count.
*
*
*Entry:
*	char *dest - pointer to destination
*	char *source - source string for copy
*	unsigned count - max number of characters to copy
*
*Exit:
*	returns dest
*
*Exceptions:
*
*******************************************************************************/

char * __cdecl strncpy (
	char * dest,
	const char * source,
	size_t count
	)
{
	char *start = dest;

	while (count && (*dest++ = *source++))	  /* copy string */
		count--;

	if (count)				/* pad out with zeroes */
		while (--count)
			*dest++ = '\0';

	return(start);
}
