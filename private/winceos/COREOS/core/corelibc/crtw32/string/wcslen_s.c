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
*wcslen_s.c - contains wcsnlen() routine
*
*Purpose:
*   wcslen returns the length of a null-terminated wide-character string,
*   not including the null wchar_t itself.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>

/***
*wcsnlen - return the length of a null-terminated wide-character string
*
*Purpose:
*   Finds the length in bytes of the given string, not including
*   the final null character. Only the first maxsize characters
*   are inspected: if the null character is not found, maxsize is
*   returned.
*
*Entry:
*   const wchar_t * wcs - string whose length is to be computed
*   size_t maxsize
*
*Exit:
*   Length of the string "wcs", exclusive of the final null byte, or
*   maxsize if the null character is not found.
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl wcsnlen(const wchar_t *wcs, size_t maxsize)
{
    size_t n;

    /* Note that we do not check if s == NULL, because we do not
     * return errno_t...
     */

    for (n = 0; n < maxsize && *wcs; n++, wcs++)
        ;

    return n;
}

#endif /* _POSIX_ */
