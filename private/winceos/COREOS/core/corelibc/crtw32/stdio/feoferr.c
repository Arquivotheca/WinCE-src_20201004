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
*feoferr.c - defines feof() and ferror()
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Defines feof() (test for end-of-file on a stream) and ferror() (test
*	for error on a stream).
*
*Revision History:
*	03-13-89  GJF	Module created
*	03-27-89  GJF	Moved to 386 tree
*	02-15-90  GJF	Fixed copyright
*	03-16-90  GJF	Made calling type  _CALLTYPE1 and added #include
*			<cruntime.h>.
*	10-02-90  GJF	New-style function declarators.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	09-11-03  SJ	Secure CRT Work - Assertions & Validations
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <internal.h>

/* remove macro definitions for feof() and ferror()
 */
#undef	feof
#undef	ferror

/***
*int feof(stream) - test for end-of-file on stream
*
*Purpose:
*	Tests whether or not the given stream is at end-of-file. Normally
*	feof() is a macro, but it must also be available as a true function
*	for ANSI.
*
*Entry:
*	FILE *stream - stream to test
*
*Exit:
*	returns nonzero (_IOEOF to be more precise) if and only if the stream
*	is at end-of-file
*
*Exceptions:
*
*******************************************************************************/

int __cdecl feof (
	FILE *stream
	)
{
	_VALIDATE_RETURN((stream != NULL),EINVAL, 0);
	return( ((stream)->_flag & _IOEOF) );
}


/***
*int ferror(stream) - test error indicator on stream
*
*Purpose:
*	Tests the error indicator for the given stream. Normally, feof() is
*	a macro, but it must also be available as a true function for ANSI.
*
*Entry:
*	FILE *stream - stream to test
*
*Exit:
*	returns nonzero (_IOERR to be more precise) if and only if the error
*	indicator for the stream is set.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl ferror (
	FILE *stream
	)
{
	_VALIDATE_RETURN((stream != NULL),EINVAL, 0);
	return( ((stream)->_flag & _IOERR) );
}
