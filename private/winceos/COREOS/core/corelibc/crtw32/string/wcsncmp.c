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
#pragma function(wcsncmp)
/***
*int wcsncmp(first, last, count) - compare first count chars of wchar_t strings
*
*Purpose:
*   Compares two strings for lexical order.  The comparison stops
*   after: (1) a difference between the strings is found, (2) the end
*   of the strings is reached, or (3) count characters have been
*   compared (wide-character strings).
*
*Entry:
*   wchar_t *first, *last - strings to compare
*   size_t count - maximum number of characters to compare
*
*Exit:
*   returns <0 if first < last
*   returns  0 if first == last
*   returns >0 if first > last
*
*Exceptions:
*
*******************************************************************************/

int wcsncmp(const wchar_t * first, const wchar_t * last, size_t count) {
    if (!count)
        return(0);
    while (--count && *first && *first == *last) {
        first++;
        last++;
    }
    return((int)(*first - *last));
}

