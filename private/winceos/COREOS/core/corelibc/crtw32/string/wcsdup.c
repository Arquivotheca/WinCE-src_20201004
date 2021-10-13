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
*wcsdup.c - duplicate a wide-character string in malloc'd memory
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _wcsdup() - grab new memory, and duplicate the string into it
*	(wide-character).
*
*Revision History:
*	09-09-91  ETC	Created from strdup.c.
*	04-07-92  KRS	Updated and ripped out _INTL switches.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <malloc.h>
#include <string.h>
#include <internal.h>

/***
*wchar_t *_wcsdup(string) - duplicate string into malloc'd memory
*
*Purpose:
*	Allocates enough storage via malloc() for a copy of the
*	string, copies the string into the new memory, and returns
*	a pointer to it (wide-character).
*
*Entry:
*	wchar_t *string - string to copy into new memory
*
*Exit:
*	returns a pointer to the newly allocated storage with the
*	string in it.
*
*	returns NULL if enough memory could not be allocated, or
*	string was NULL.
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

#ifdef _DEBUG

#include <crtdbg.h>

wchar_t * __cdecl _wcsdup (
	const wchar_t * string
	)
{
	return _wcsdup_dbg(string, _NORMAL_BLOCK, NULL, 0);
}

wchar_t * __cdecl _wcsdup_dbg (
	const wchar_t * string,
	int nBlockUse,
	const char * szFileName,
	int nLine
	)

#else

wchar_t * __cdecl _wcsdup (
	const wchar_t * string
	)

#endif /* _DEBUG */

{
	wchar_t *memory;
    size_t size = 0;

	if (!string)
		return(NULL);

    size = wcslen(string) + 1;
#ifdef _DEBUG
	if (memory = (wchar_t *) _calloc_dbg(size, sizeof(wchar_t), nBlockUse, szFileName, nLine))
#else
	if (memory = (wchar_t *) calloc(size, sizeof(wchar_t)))
#endif
	{
		_ERRCHECK(wcscpy_s(memory, size, string));
        return memory;
	}

	return(NULL);
}

#endif /* _POSIX_ */
