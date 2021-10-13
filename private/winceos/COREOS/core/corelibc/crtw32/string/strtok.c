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
*strtok.c - tokenize a string with given delimiters
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines strtok() - breaks string into series of token
*	via repeated calls.
*
*Revision History:
*	06-01-89  JCR	C version created.
*	02-27-90  GJF	Fixed calling type, #include <cruntime.h>, fixed
*			copyright.
*	08-14-90  SBM	Removed now redundant #include <stddef.h>
*	10-02-90  GJF	New-style function declarator.
*	07-17-91  GJF	Multi-thread support for Win32 [_WIN32_].
*	10-26-91  GJF	Fixed nasty bug - search for end-of-token could run
*			off the end of the string.
*	02-17-93  GJF	Changed for new _getptd().
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	05-25-93  GJF	Revised to use unsigned char * pointers to access
*			the token and delimiter strings.
*	09-03-93  GJF	Replaced MTHREAD with _MT.
*	10-09-03  AC 	Added secure version.
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#ifdef _SECURE_VERSION
#include <internal.h>
#else
#include <mtdll.h>
#endif

/***
*char *strtok(string, control) - tokenize string with delimiter in control
*
*Purpose:
*	strtok considers the string to consist of a sequence of zero or more
*	text tokens separated by spans of one or more control chars. the first
*	call, with string specified, returns a pointer to the first char of the
*	first token, and will write a null char into string immediately
*	following the returned token. subsequent calls with zero for the first
*	argument (string) will work thru the string until no tokens remain. the
*	control string may be different from call to call. when no tokens remain
*	in string a NULL pointer is returned. remember the control chars with a
*	bit map, one bit per ascii char. the null char is always a control char.
*
*Entry:
*	char *string - string to tokenize, or NULL to get next token
*	char *control - string of characters to use as delimiters
*
*Exit:
*	returns pointer to first token in string, or if string
*	was NULL, to next token
*	returns NULL when no more tokens remain.
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

#ifdef _SECURE_VERSION
#define _TOKEN *context
#else
#define _TOKEN ptd->_token
#endif

#ifdef _SECURE_VERSION
char * __cdecl strtok_s (
	char * string,
	const char * control,
	char ** context
	)
#else
char * __cdecl strtok (
	char * string,
	const char * control
	)
#endif
{
	unsigned char *str;
	const unsigned char *ctrl = control;

	unsigned char map[32];
	int count;

#ifdef _SECURE_VERSION

	/* validation section */
	_VALIDATE_RETURN(context != NULL, EINVAL, NULL);
	_VALIDATE_RETURN(string != NULL || *context != NULL, EINVAL, NULL);
	_VALIDATE_RETURN(control != NULL, EINVAL, NULL);

	/* no static storage is needed for the secure version */

#else

	_ptiddata_persist ptd = _getptd_persist();

#endif

	/* Clear control map */
	for (count = 0; count < 32; count++)
		map[count] = 0;

	/* Set bits in delimiter table */
	do {
		map[*ctrl >> 3] |= (1 << (*ctrl & 7));
	} while (*ctrl++);

	/* Initialize str */
	
	/* If string is NULL, set str to the saved
	 * pointer (i.e., continue breaking tokens out of the string
	 * from the last strtok call) */
	if (string)
		str = string;
	else
		str = _TOKEN;

	/* Find beginning of token (skip over leading delimiters). Note that
	 * there is no token iff this loop sets str to point to the terminal
	 * null (*str == '\0') */
	while ( (map[*str >> 3] & (1 << (*str & 7))) && *str )
		str++;

	string = str;

	/* Find the end of the token. If it is not the end of the string,
	 * put a null there. */
	for ( ; *str ; str++ )
		if ( map[*str >> 3] & (1 << (*str & 7)) ) {
			*str++ = '\0';
			break;
		}

	/* Update nextoken (or the corresponding field in the per-thread data
	 * structure */
	_TOKEN = str;

	/* Determine if a token has been found. */
	if ( string == str )
		return NULL;
	else
		return string;
}
