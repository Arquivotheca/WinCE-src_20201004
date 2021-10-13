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
*long _wtol(wchar_t *nptr) - Convert string to long
*
*Purpose:
*   Converts ASCII string pointed to by nptr to binary.
*   Overflow is not detected.
*
*Entry:
*   nptr = ptr to string to convert
*
*Exit:
*   return long int value of the string
*
*Exceptions:
*   None - overflow is not detected.
*
*******************************************************************************/

long _wtol(const wchar_t *nptr) {
    wchar_t c;            /* current char */
    long total=0;       /* current total */
    int sign, digit;        /* if '-', then negative, otherwise positive */
    /* skip whitespace */
    while (iswspace(*nptr))
        ++nptr;
    // check for hex "0x"
    if (TEXT('0') == *nptr)
        nptr++;
    if (TEXT('x') == *nptr || TEXT('X') == *nptr) {
        nptr++; // skip the x
        while (c=*nptr++) {
            if (c >= TEXT('0') && c <= TEXT('9'))
                digit = c - TEXT('0');
            else if (c >= TEXT('a') && c <= TEXT('f'))
                digit = (c - TEXT('a')) + 10;
            else if (c >= TEXT('A') && c <= TEXT('F'))
                digit = (c - TEXT('A')) + 10;
            else // invalid character
                break;
            total = 16 * total + digit;
        }
        return total;
    }
    // We are in decimal land ...
    c = *nptr++;
    sign = c;       /* save sign indication */
    if (c == TEXT('-') || c == TEXT('+'))
        c = *nptr++;    /* skip sign */
    while (c >= TEXT('0') && c <= TEXT('9')) {
        total = 10 * total + (c - TEXT('0'));   /* accumulate digit */
        c = *nptr++;    /* get next char */
    }
    if (sign == TEXT('-'))
        return -total;
    else
        return total;   /* return result, negated if necessary */
}

