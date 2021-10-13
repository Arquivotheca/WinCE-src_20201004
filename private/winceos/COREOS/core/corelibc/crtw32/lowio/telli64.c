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
*telli64.c - find file position
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	contains _telli64 - find file position
*
*Revision History:
*	11-18-94  GJF	Created. Adapted from tell.c
*
*******************************************************************************/

#include <cruntime.h>
#include <io.h>
#include <stdio.h>

/***
*__int64 _telli64(filedes) - find file position
*
*Purpose:
*	Gets the current position of the file pointer (no adjustment
*	for buffering).
*
*Entry:
*	int filedes - file handle of file
*
*Exit:
*	returns file position or -1i64 (sets errno) if bad file descriptor or
*	pipe
*
*Exceptions:
*
*******************************************************************************/

__int64 __cdecl _telli64 (
	int filedes
	)
{
	return( _lseeki64( filedes, 0i64, SEEK_CUR ) );
}
