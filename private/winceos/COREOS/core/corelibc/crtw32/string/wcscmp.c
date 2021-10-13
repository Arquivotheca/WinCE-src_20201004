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
#pragma function(wcscmp)

/***
*wcscmp - compare two wchar_t strings,
*    returning less than, equal to, or greater than
*
*Purpose:
*   wcscmp compares two wide-character strings and returns an integer
*   to indicate whether the first is less than the second, the two are
*   equal, or whether the first is greater than the second.
*
*   Comparison is done wchar_t by wchar_t on an UNSIGNED basis, which is to
*   say that Null wchar_t(0) is less than any other character.
*
*Entry:
*   const wchar_t * src - string for left-hand side of comparison
*   const wchar_t * dst - string for right-hand side of comparison
*
*Exit:
*   returns -1 if src <  dst
*   returns  0 if src == dst
*   returns +1 if src >  dst
*
*Exceptions:
*
*******************************************************************************/

int wcscmp (const wchar_t *src, const wchar_t *dst) {
    int ret;
    while((*src == *dst) && *dst)
        ++src, ++dst;
    ret = *src - *dst;
    return (ret > 0 ? 1 : ret < 0 ? -1 : 0);
}
