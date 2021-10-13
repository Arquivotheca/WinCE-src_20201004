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
*lsearch.c - linear search of an array
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	contains the _lsearch() function - linear search of an array
*
*Revision History:
*	06-19-85  TC	initial version
*	05-14-87  JMB	added function pragma for memcpy in compact/large mode
*			for huge pointer support
*			include sizeptr.h for SIZED definition
*	08-01-87  SKS	Add include file for prototype of memcpy()
*	12-11-87  JCR	Added "_LOAD_DS" to declaration
*	01-21-88  JCR	Backed out _LOAD_DS...
*	10-30-89  JCR	Added _cdecl to prototypes
*	03-14-90  GJF	Replaced _cdecl with _CALLTYPE1, added #include
*			<cruntime.h>, removed #include <register.h> and
*			fixed the copyright. Also, cleaned up the formatting
*			a bit.
*	04-05-90  GJF	Added #include <search.h> and fixed the resulting
*			compiler errors and warnings. Removed unreferenced
*			local variable. Also, removed #include <sizeptr.h>.
*	07-25-90  SBM	Replaced <stdio.h> by <stddef.h>
*	10-04-90  GJF	New-style function declarator.
*	01-17-91  GJF	ANSI naming.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	08-11-03  AC 	Added safe version (_lsearch_s)
*       10-31-04  MSL   Enable __clrcall safe version
*
*******************************************************************************/

#include <cruntime.h>
#include <stddef.h>
#include <search.h>
#include <memory.h>
#include <internal.h>

#if defined(_M_CEE)
#define __fileDECL  __clrcall
#else
#define __fileDECL  __cdecl
#endif
/***
*char *_lsearch(key, base, num, width, compare) - do a linear search
*
*Purpose:
*	Performs a linear search on the array, looking for the value key
*	in an array of num elements of width bytes in size.  Returns
*	a pointer to the array value if found; otherwise adds the
*	key to the end of the list.
*
*Entry:
*	char *key - key to search for
*	char *base - base of array to search
*	unsigned *num - number of elements in array
*	int width - number of bytes in each array element
*	int (*compare)() - pointer to function that compares two
*		array values, returning 0 if they are equal and non-0
*		if they are different. Two pointers to array elements
*		are passed to this function.
*
*Exit:
*	if key found:
*		returns pointer to array element
*	if key not found:
*		adds the key to the end of the list, and increments
*		*num.
*		returns pointer to new element.
*
*Exceptions:
*	Input parameters are validated. Refer to the validation section of the function. 
*
*******************************************************************************/

#ifdef __USE_CONTEXT
#define __COMPARE(context, p1, p2) (*compare)(context, p1, p2)
#else
#define __COMPARE(context, p1, p2) (*compare)(p1, p2)
#endif

#if !defined(_M_CEE)
_CRTIMP
#endif
#ifdef __USE_CONTEXT
void *__fileDECL _lsearch_s (
	const void *key,
	void *base,
	unsigned int *num,
	size_t width,
	int (__fileDECL *compare)(void *, const void *, const void *),
	void *context
	)
#else
void *__fileDECL _lsearch (
	const void *key,
	void *base,
	unsigned int *num,
	unsigned int width,
	int (__fileDECL *compare)(const void *, const void *)
	)
#endif
{
	unsigned int place = 0;

	/* validation section */
	_VALIDATE_RETURN(key != NULL, EINVAL, NULL);
	_VALIDATE_RETURN(num != NULL, EINVAL, NULL);
	_VALIDATE_RETURN(base != NULL, EINVAL, NULL);
	_VALIDATE_RETURN(width > 0, EINVAL, NULL);
	_VALIDATE_RETURN(compare != NULL, EINVAL, NULL);
	
	while (place < *num)
	{
		if (__COMPARE(context, key, base) == 0)
		{
			return base;
		}
		else
		{
			base = (char*)base + width;
			place++;
		}
	}
	memcpy(base, key, width);
	(*num)++;
	return base;
}

#undef __fileDECL
#undef __COMPARE
