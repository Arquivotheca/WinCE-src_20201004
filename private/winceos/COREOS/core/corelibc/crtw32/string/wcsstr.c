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

/******************************************************************************
*wchar_t *wcsstr(string1, string2) - search for string2 in string1
*   (wide strings)
*
*Purpose:
*   finds the first occurrence of string2 in string1 (wide strings)
*
*Entry:
*   wchar_t *string1 - string to search in
*   wchar_t *string2 - string to search for
*
*Exit:
*   returns a pointer to the first occurrence of string2 in
*   string1, or NULL if string2 does not occur in string1
*
*Uses:
*
*Exceptions:
*
********************************************************************************/

wchar_t * wcsstr (const wchar_t * wcs1, const wchar_t * wcs2) {
    wchar_t *cp = (wchar_t *) wcs1;
    wchar_t *s1, *s2;
    if (!*wcs2)
        return((wchar_t *)wcs1);
    while (*cp) {
        s1 = cp;
        s2 = (wchar_t *) wcs2;
        while ( *s1 && *s2 && !(*s1-*s2) )
            s1++, s2++;
        if (!*s2)
            return(cp);
        cp++;
    }
    return(NULL);
}

