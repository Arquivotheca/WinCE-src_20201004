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
*fputwchr.c - write a wide character to stdout
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _fputwchar(), putwchar() - write a wide character to stdout,
*	function version
*
*Revision History:
*	04-26-93  CFW	Module created.
*	04-30-93  CFW	Bring wide char support from fputchar.c.
*	06-02-93  CFW	Wide get/put use wint_t.
*       02-07-94  CFW   POSIXify.
*       11-22-00  PML   Wide-char *putwc* functions take a wchar_t, not wint_t.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <stdio.h>
#include <tchar.h>

/***
*wint_t _fputwchar(ch), putwchar() - put a wide character to stdout
*
*Purpose:
*	Puts the given wide character to stdout.  Function version of macro
*	putwchar().
*
*Entry:
*	wchar_t ch - character to output
*
*Exit:
*	returns character written if successful
*	returns WEOF if fails
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl _fputwchar (
	wchar_t ch
	)
{
	return(putwc(ch, stdout));
}

#undef putwchar

wint_t __cdecl putwchar (
	wchar_t ch
	)
{
	return(_fputwchar(ch));
}

#endif /* _POSIX_ */
