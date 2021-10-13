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
*strcmp.c - routine to compare two strings (for equal, less, or greater)
*
*Purpose:
*   Compares two string, determining their lexical order.
*
*******************************************************************************/

#include <corecrt.h>
#include <string.h>

#ifdef  _MSC_VER
#pragma function(strcmp)
#endif

/***
*strcmp - compare two strings, returning less than, equal to, or greater than
*
*Purpose:
*   STRCMP compares two strings and returns an integer
*   to indicate whether the first is less than the second, the two are
*   equal, or whether the first is greater than the second.
*
*   Comparison is done byte by byte on an UNSIGNED basis, which is to
*   say that Null (0) is less than any other character (1-255).
*
*Entry:
*   const char * src - string for left-hand side of comparison
*   const char * dst - string for right-hand side of comparison
*
*Exit:
*   returns -1 if src <  dst
*   returns  0 if src == dst
*   returns +1 if src >  dst
*
*Exceptions:
*
*******************************************************************************/

int __cdecl strcmp (
    const char * src,
    const char * dst
    )
{
    int ret = 0 ;

    while( ! (ret = *(unsigned char *)src - *(unsigned char *)dst) && *dst)
        ++src, ++dst;

    if ( ret < 0 )
        ret = -1 ;
    else if ( ret > 0 )
        ret = 1 ;

    return( ret );
}
