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
*fputchar.c - write a character to stdout
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _fputchar(), putchar() - write a character to stdout, function version
*	defines _fputwchar(), putwchar() - write a wide character to stdout, function version
*
*Revision History:
*	11-30-83  RN	initial version
*	11-09-87  JCR	Multi-thread support
*	12-11-87  JCR	Added "_LOAD_DS" to declaration
*	05-31-88  PHG	Merged DLL and normal versions
*	06-21-89  PHG	Added putchar() function
*	02-15-90  GJF	Fixed copyright and indents
*	03-19-90  GJF	Replaced _LOAD_DS with _CALLTYPE1, added #include
*			<cruntime.h> and removed #include <register.h>.
*	10-02-90  GJF	New-style function declarators.
*	01-21-91  GJF	ANSI naming.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	04-26-93  CFW	Wide char enable.
*	04-30-93  CFW	Move wide char support to fputwchr.c.
*	03-15-95  GJF	Deleted #include <tchar.h>
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>

/***
*int _fputchar(ch), putchar() - put a character to stdout
*
*Purpose:
*	Puts the given characters to stdout.  Function version of macro
*	putchar().
*
*Entry:
*	int ch - character to output
*
*Exit:
*	returns character written if successful
*	returns EOF if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _fputchar (
	int ch
	)
{
	return(putc(ch, stdout));
}

#undef putchar

int __cdecl putchar (
	int ch
	)
{
	return _fputchar(ch);
}
