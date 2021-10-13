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

#include <stdlib.h>

#pragma warning(disable:4163)
#pragma function(wcscat)

/***
*wchar_t *wcscat(dst, src) - concatenate (append) one wchar_t string to another
*
*Purpose:
*   Concatenates src onto the end of dest.  Assumes enough
*   space in dest.
*
*Entry:
*   wchar_t *dst - wchar_t string to which "src" is to be appended
*   const wchar_t *src - wchar_t string to be appended to the end of "dst"
*
*Exit:
*   The address of "dst"
*
*Exceptions:
*
*******************************************************************************/

wchar_t * wcscat(wchar_t *dst, const wchar_t *src) {
    wchar_t * cp = dst;
    while( *cp )
        cp++;           /* find end of dst */
    while((*cp++ = *src++) != 0)
        ;   /* Copy src to end of dst */
    return( dst );          /* return dst */
}

