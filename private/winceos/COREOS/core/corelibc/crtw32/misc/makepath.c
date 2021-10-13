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
*makepath.c - create path name from components
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	To provide support for creation of full path names from components
*
*Revision History:
*	06-13-87  DFW	initial version
*	08-05-87  JCR	Changed appended directory delimeter from '/' to '\'.
*	09-24-87  JCR	Removed 'const' from declarations (caused cl warnings).
*	12-11-87  JCR	Added "_LOAD_DS" to declaration
*	11-20-89  GJF	Fixed copyright, indents. Added const to types of
*			appropriate args.
*	03-14-90  GJF	Replaced _LOAD_DS with _CALLTYPE1 and added #include
*			<cruntime.h>.
*	10-04-90  GJF	New-style function declarator.
*	06-09-93  KRS	Add _MBCS support.
*	12-07-93  CFW	Wide char enable.
*	09-11-03  SJ	Secure CRT Work - Assertions & Validations
*	10-20-03  SJ	errno shouldn't be set to 0 on success
*	08-05-04  AC	Moved secure version in tmakepath_s.inl and makepath_s.c
*	11-11-04  AC	Using (size_t)-1 instead of UINT_MAX
*
*******************************************************************************/

#include <mtdll.h>
#include <cruntime.h>
#include <stdlib.h>
#ifdef _MBCS
#include <mbdata.h>
#include <mbstring.h>
#endif
#include <tchar.h>
#include <internal.h>
#include <limits.h>

/***
*void _makepath - build path name from components
*
*Purpose:
*	create a path name from its individual components
*
*Entry:
*	_TSCHAR *path  - pointer to buffer for constructed path
*	_TSCHAR *drive - pointer to drive component, may or may not contain
*		      trailing ':'
*	_TSCHAR *dir   - pointer to subdirectory component, may or may not include
*		      leading and/or trailing '/' or '\' characters
*	_TSCHAR *fname - pointer to file base name component
*	_TSCHAR *ext   - pointer to extension component, may or may not contain
*		      a leading '.'.
*
*Exit:
*	path - pointer to constructed path name
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _tmakepath (
	register _TSCHAR *path,
	const _TSCHAR *drive,
	const _TSCHAR *dir,
	const _TSCHAR *fname,
	const _TSCHAR *ext
	)
{
	_tmakepath_s(path, (size_t)-1, drive, dir, fname, ext);
}
