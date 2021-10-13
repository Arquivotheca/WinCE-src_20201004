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
*wchar_t *wcspbrk(string, control) - scans string for a character from control
*
*Purpose:
*   Returns pointer to the first wide-character in
*   a wide-character string in the control string.
*
*Entry:
*   wchar_t *string - string to search in
*   wchar_t *control - string containing characters to search for
*
*Exit:
*   returns a pointer to the first character from control found
*   in string.
*   returns NULL if string and control have no characters in common.
*
*Exceptions:
*
*******************************************************************************/

wchar_t * wcspbrk(const wchar_t * string, const wchar_t * control) {
    wchar_t *wcset;
    /* 1st char in control string stops search */
    while (*string) {
        for (wcset = (wchar_t *) control; *wcset; wcset++)
            if (*wcset == *string)
                return (wchar_t *) string;
        string++;
    }
    return NULL;
}

