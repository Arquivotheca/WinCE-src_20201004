//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
