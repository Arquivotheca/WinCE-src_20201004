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
*wchar_t *wcsncat(front, back, count) - append count chars of back onto front
*
*Purpose:
*   Appends at most count characters of the string back onto the
*   end of front, and ALWAYS terminates with a null character.
*   If count is greater than the length of back, the length of back
*   is used instead.  (Unlike wcsncpy, this routine does not pad out
*   to count characters).
*
*Entry:
*   wchar_t *front - string to append onto
*   wchar_t *back - string to append
*   size_t count - count of max characters to append
*
*Exit:
*   returns a pointer to string appended onto (front).
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

wchar_t * wcsncat(wchar_t * front, const wchar_t * back, size_t count) {
    wchar_t *start = front;
    while (*front++)
        ;
    front--;
    while (count--)
        if ((*front++ = *back++) == 0)
            return(start);
    *front = L'\0';
    return(start);
}

