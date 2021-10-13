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
*abs.c - find absolute value
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines abs() - find the absolute value of an int.
*
*Revision History:
*       04-22-87  JMB   added function pragma for conversion to C 5.0 compiler
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       03-14-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and fixed the copyright. Also, cleaned
*                       up the formatting a bit.
*       10-04-90  GJF   New-style function declarator.
*       12-28-90  SRW   Added _CRUISER_ conditional around function pragma
*       04-01-91  SRW   Enable #pragma function for i386 _WIN32_ builds too.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*                       No _CRTIMP for CRT DLL model due to intrinsic
*       12-03-93  GJF   Turn on #pragma function for all MS front-ends (esp.,
*                       Alpha compiler).
*       12-30-94  JCF   Turn off #pragma function for MAC.
*       05-17-99  PML   Remove all Macintosh support.
*       01-04-01  GB    Added __int64 version of abs
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>

#pragma function(abs, _abs64)

/***
*int abs(number) - find absolute value of number
*
*Purpose:
*       Returns the absolute value of number (if number >= 0, returns number,
*       else returns -number).
*
*Entry:
*       int number - number to find absolute value of
*
*Exit:
*       returns the aboslute value of number
*
*Exceptions:
*
*******************************************************************************/

int __cdecl abs (
        int number
        )
{
        return( number>=0 ? number : -number );
}
__int64 __cdecl _abs64(
        __int64 num
        )
{
        return (num >=0 ? num : -num);
}
