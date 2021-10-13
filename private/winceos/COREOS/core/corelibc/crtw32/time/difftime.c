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
*difftime.c - return difference between two times as a double
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Find difference between two time in seconds.
*
*Revision History:
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       08-15-89  PHG   Made MTHREAD version _pascal
*       11-20-89  JCR   difftime() always _cdecl (not pascal even under
*                       mthread)
*       03-20-90  GJF   Replaced _LOAD_DS with CALLTYPE1, added #include
*                       <cruntime.h> and fixed the copyright. Also, cleaned
*                       up the formatting a bit.
*       10-04-90  GJF   New-style function declarator.
*       05-19-92  DJM   ifndef for POSIX build.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       08-30-99  PML   Fix function header comment, detab.
*       02-04-03  EAN   Changed to _difftime64 as part of the 64-bit time_t change
*       03-06-03  SSM   Assertions and validations
*
*******************************************************************************/

#ifndef _POSIX_

#ifdef _WIN32_WCE
//crtw32/h/time.inl has defined difftime() body
#define _NOUSE_INLINE_WCE
#endif // _WIN32_WCE

#include <cruntime.h>
#include <time.h>
#include <internal.h>

/***
*double _difftime32(b, a) - find difference between two times
*
*Purpose:
*       returns difference between two times (b-a)
*
*Entry:
*       __time32_t a, b - times to difference
*
*Exit:
*       returns a double with the time in seconds between two times
*       0 if input is invalid
*
*Exceptions:
*
*******************************************************************************/

double __cdecl _difftime32 (
        __time32_t b,
        __time32_t a
        )
{
        _VALIDATE_RETURN_NOEXC(
            ( ( a >= 0 ) && ( b >= 0 ) ),
            EINVAL,
            0
        )

        return( (double)( b - a ) );
}

#ifdef _WIN32_WCE
double __cdecl difftime(time_t _Time1, time_t _Time2)
{
    return _difftime32(_Time1,_Time2);
}
#endif // _WIN32_WCE
#endif  /* _POSIX_ */
