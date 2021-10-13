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
*strncmp.c - compare first n characters of two strings
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines strncmp() - compare first n characters of two strings
*	for lexical order.
*
*Revision History:
*	05-31-89   JCR	C version created.
*	02-27-90   GJF	Fixed calling type, #include <cruntime.h>, fixed
*			copyright.
*	10-02-90   GJF	New-style function declarator.
*	10-11-91   GJF	Update! Comparison of final bytes must use unsigned
*			chars.
*	09-02-93   GJF	Replaced _CALLTYPE1 with __cdecl.
*   07-10-02   MSL  Removed x86 assembly version (did an unnecessary strlen)
*                   Also substituted our version for Vinod Grover's optimised 
*                   version
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>

#if defined(_M_ARM)
#pragma function(strncmp)
#endif

/***
*int strncmp(first, last, count) - compare first count chars of strings
*
*Purpose:
*	Compares two strings for lexical order.  The comparison stops
*	after: (1) a difference between the strings is found, (2) the end
*	of the strings is reached, or (3) count characters have been
*	compared.
*
*Entry:
*	char *first, *last - strings to compare
*	unsigned count - maximum number of characters to compare
*
*Exit:
*	returns <0 if first < last
*	returns  0 if first == last
*	returns >0 if first > last
*
*Exceptions:
*
*******************************************************************************/

int __cdecl strncmp
(
    const char *first, 
    const char *last, 
    size_t      count
)
{
    size_t x = 0;

    if (!count)
    {
        return 0;
    }

    /*
     * This explicit guard needed to deal correctly with boundary
     * cases: strings shorter than 4 bytes and strings longer than
     * UINT_MAX-4 bytes .
     */
    if( count >= 4 )
    {
        /* unroll by four */
        for (; x < count-4; x+=4)
        {
            first+=4;
            last +=4;

            if (*(first-4) == 0 || *(first-4) != *(last-4)) 
            {
                return(*(unsigned char *)(first-4) - *(unsigned char *)(last-4));
            }

            if (*(first-3) == 0 || *(first-3) != *(last-3)) 
            {
                return(*(unsigned char *)(first-3) - *(unsigned char *)(last-3));
            }

            if (*(first-2) == 0 || *(first-2) != *(last-2)) 
            {
                return(*(unsigned char *)(first-2) - *(unsigned char *)(last-2));
            }

            if (*(first-1) == 0 || *(first-1) != *(last-1)) 
            {
                return(*(unsigned char *)(first-1) - *(unsigned char *)(last-1));
            }
        }
    }
    
    /* residual loop */
    for (; x < count; x++) 
    {
        if (*first == 0 || *first != *last)
        {
            return(*(unsigned char *)first - *(unsigned char *)last);
        }
        first+=1;
        last+=1;
    }
  
    return 0;
}
