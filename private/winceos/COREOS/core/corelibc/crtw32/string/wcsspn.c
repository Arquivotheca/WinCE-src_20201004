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
*int wcsspn(string, control) - find init substring of control chars
*
*Purpose:
*   Finds the index of the first character in string that does belong
*   to the set of characters specified by control.  This is
*   equivalent to the length of the initial substring of string that
*   consists entirely of characters from control.  The L'\0' character
*   that terminates control is not considered in the matching process
*   (wide-character strings).
*
*Entry:
*   wchar_t *string - string to search
*   wchar_t *control - string containing characters not to search for
*
*Exit:
*   returns index of first wchar_t in string not in control
*
*Exceptions:
*
*******************************************************************************/

size_t wcsspn(const wchar_t * string, const wchar_t * control) {
    wchar_t *str = (wchar_t *) string;
    wchar_t *ctl;
    /* 1st char not in control string stops search */
    while (*str) {
        for (ctl = (wchar_t *)control; *ctl != *str; ctl++)
            if (*ctl == 0) /* reached end of control string without finding a match */
                return str - string;
        str++;
    }
    /* The whole string consisted of characters from control */
    return str - string;
}

