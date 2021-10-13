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
*crt0fp.asm - floating point not loaded trap
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	To trap certain cases where certain necessary floating-point
*	software is not loaded.  Two specific cases are when no emulator
*	is linked in but no coprocessor is present, and when floating
*	point i/o conversions are done, but no floating-point variables
*	or expressions are used in the program.
*
*Revision History:
*	06-29-89  PHG	module created, based on asm version
*	04-09-90  GJF	Added #include <cruntime.h>. Made calling type
*			_CALLTYPE1. Also, fixed the copyright.
*	04-10-90  GJF	Fixed compiler warnings (-W3).
*	10-08-90  GJF	New-style function declarator.
*	10-11-90  GJF	Changed _amsg_exit() interface.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <rterr.h>

/***
*_fptrap - trap for missing floating-point software
*
*Purpose:
*	Catches these cases of incomplete f.p. software linked into a program.
*
*	(1) no coprocessor present, and no emulator linked in
*
*	(2) "%e", "%f", and "%g" i/o conversion formats specified, but
*	    not all conversion software has been linked in, because the
*	    program did not use any floating-point variables or expressions.
*
*Entry:
*	None.
*
*Exit:
*	Never returns.
*
*Exceptions:
*	Transfers control to _amsg_exit which ...
*	- Writes error message to standard error:  "floating point not loaded"
*	- Terminates the program by calling _exit().
*******************************************************************************/

void __cdecl _fptrap(
	void
	)
{
	_amsg_exit(_RT_FLOAT);
}
