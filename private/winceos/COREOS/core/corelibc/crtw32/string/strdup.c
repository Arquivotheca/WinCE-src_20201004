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
*strdup.c - duplicate a string in malloc'd memory
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _strdup() - grab new memory, and duplicate the string into it.
*
*Revision History:
*	05-31-89   JCR	C version created.
*	02-27-90   GJF	Fixed calling type, #include <cruntime.h>, fixed
*			copyright.
*	08-14-90   SBM	Removed now redundant #include <stddef.h>
*	10-02-90   GJF	New-style function declarator.
*	01-18-91   GJF	ANSI naming.
*	09-02-93   GJF	Replaced _CALLTYPE1 with __cdecl.
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <string.h>
#include <internal.h>

/***
*char *_strdup(string) - duplicate string into malloc'd memory
*
*Purpose:
*	Allocates enough storage via malloc() for a copy of the
*	string, copies the string into the new memory, and returns
*	a pointer to it.
*
*Entry:
*	char *string - string to copy into new memory
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

char * __cdecl _strdup (
	const char * string
	)
{
	return _strdup_dbg(string, _NORMAL_BLOCK, NULL, 0);
}

char * __cdecl _strdup_dbg (
	const char * string,
	int nBlockUse,
	const char * szFileName,
	int nLine
	)

#else 

char * __cdecl _strdup (
	const char * string
	)

#endif /* _DEBUG */

{
	char *memory;
    size_t size = 0;

	if (!string)
		return(NULL);

    size = strlen(string) + 1;
#ifdef _DEBUG
	if (memory = _malloc_dbg(size, nBlockUse, szFileName, nLine))
#else
	if (memory = malloc(size))
#endif
	{
		_ERRCHECK(strcpy_s(memory, size, string));
        return memory;
	}

	return(NULL);
}
