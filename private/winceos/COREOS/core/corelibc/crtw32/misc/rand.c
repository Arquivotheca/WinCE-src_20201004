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
*rand.c - random number generator
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines rand(), srand() - random number generator
*
*Revision History:
*	03-16-84  RN	initial version
*	12-11-87  JCR	Added "_LOAD_DS" to declaration
*	05-31-88  PHG	Merged DLL and normal versions
*	06-06-89  JCR	386 mthread support
*	03-15-90  GJF	Replaced _LOAD_DS with _CALLTYPE1, added #include
*			<cruntime.h> and fixed the copyright. Also, cleaned
*			up the formatting a bit.
*	04-05-90  GJF	Added #include <stdlib.h>.
*	10-04-90  GJF	New-style function declarators.
*	07-17-91  GJF	Multi-thread support for Win32 [_WIN32_].
*	02-17-93  GJF	Changed for new _getptd().
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	09-06-94  CFW	Remove Cruiser support.
*	09-06-94  CFW	Replace MTHREAD with _MT.
*
*******************************************************************************/

#include <cruntime.h>
#include <mtdll.h>
#include <stddef.h>
#include <stdlib.h>

/***
*void srand(seed) - seed the random number generator
*
*Purpose:
*	Seeds the random number generator with the int given.  Adapted from the
*	BASIC random number generator.
*
*Entry:
*	unsigned seed - seed to seed rand # generator with
*
*Exit:
*	None.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl srand (
	unsigned int seed
	)
{
	_getptd_persist()->_holdrand = (unsigned long)seed;
}


/***
*int rand() - returns a random number
*
*Purpose:
*	returns a pseudo-random number 0 through 32767.
*
*Entry:
*	None.
*
*Exit:
*	Returns a pseudo-random number 0 through 32767.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl rand (
	void
	)
{
	_ptiddata_persist ptd = _getptd_persist();

	return( ((ptd->_holdrand = ptd->_holdrand * 214013L
	    + 2531011L) >> 16) & 0x7fff );
}
