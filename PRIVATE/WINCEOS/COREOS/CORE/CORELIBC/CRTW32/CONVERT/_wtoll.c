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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include <stdlib.h>

#define TEXT(x) L##x

/***
*long _wtoll(wchar_t *nptr) - Convert string to 64 bit integer
*
*Purpose:
*   Converts ASCII string pointed to by nptr to binary.
*   Overflow is not detected.
*
*Entry:
*   nptr = ptr to string to convert
*
*Exit:
*   return __int64 value of the string
*
*Exceptions:
*   None - overflow is not detected.
*
*******************************************************************************/

__int64 _wtoll(const wchar_t *nptr) {
    wchar_t c;            /* current char */
    __int64 total;      /* current total */
    int sign;       /* if '-', then negative, otherwise positive */
    /* skip whitespace */
    while (iswspace(*nptr))
        ++nptr;
    c = *nptr++;
    sign = c;       /* save sign indication */
    if (c == TEXT('-') || c == TEXT('+'))
        c = *nptr++;    /* skip sign */
    total = 0;
    while (c >= TEXT('0') && c <= TEXT('9')) {
        total = 10 * total + (c - TEXT('0'));   /* accumulate digit */
        c = *nptr++;    /* get next char */
    }
    if (sign == TEXT('-'))
        return -total;
    else
        return total;   /* return result, negated if necessary */
}

