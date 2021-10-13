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

/***
*wchar_t *wcsrchr(string, ch) - find last occurrence of ch in wide string
*
*Purpose:
*   Finds the last occurrence of ch in string.  The terminating
*   null character is used as part of the search (wide-characters).
*
*Entry:
*   wchar_t *string - string to search in
*   wchar_t ch - character to search for
*
*Exit:
*   returns a pointer to the last occurrence of ch in the given
*   string
*   returns NULL if ch does not occurr in the string
*
*Exceptions:
*
*******************************************************************************/

wchar_t * wcsrchr(const wchar_t * string, wchar_t ch) {
    wchar_t *start = (wchar_t *)string;

    /* find end of string */
    while (*string++);

    /* search towards front */
    while (--string != start && *string != ch);
    if (*string == ch)          /* wchar_t found ? */
        return( (wchar_t *)string );
    return(NULL);
}

