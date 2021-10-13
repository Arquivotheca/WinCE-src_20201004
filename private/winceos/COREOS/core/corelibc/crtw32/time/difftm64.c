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
*difftm64.c - return difference between two times as a double
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Find difference between two time in seconds.
*
*Revision History:
*       02-03-03  EAN   created
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <time.h>
#include <internal.h>

/***
*double _difftime64(b, a) - find difference between two times
*
*Purpose:
*       returns difference between two times (b-a)
*
*Entry:
*       __time64_t a, b - times to difference
*
*Exit:
*       returns a double with the time in seconds between two times
*       0 if input is invalid
*
*Exceptions:
*
*******************************************************************************/

double __cdecl _difftime64 (
        __time64_t b,
        __time64_t a
        )
{
        _VALIDATE_RETURN_NOEXC(
            ( ( a >= 0 ) && ( b >= 0 ) ),
            EINVAL,
            0
        )

        return( (double)( b - a ) );
}

#endif  /* _POSIX_ */
