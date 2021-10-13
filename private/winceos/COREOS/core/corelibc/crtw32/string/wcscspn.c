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
*size_t wcscspn(string, control) - search for init substring w/o control wchars
*
*Purpose:
*   returns the index of the first character in string that belongs
*   to the set of characters specified by control.  This is equivalent
*   to the length of the length of the initial substring of string
*   composed entirely of characters not in control.  Null chars not
*   considered (wide-character strings).
*
*Entry:
*   wchar_t *string - string to search
*   wchar_t *control - set of characters not allowed in init substring
*
*Exit:
*   returns the index of the first wchar_t in string
*   that is in the set of characters specified by control.
*
*Exceptions:
*
*******************************************************************************/

size_t wcscspn (const wchar_t * string, const wchar_t * control) {
    wchar_t *str = (wchar_t *) string;
    wchar_t *wcset;
    /* 1st char in control string stops search */
    while (*str) {
        for (wcset = (wchar_t *)control; *wcset; wcset++)
            if (*wcset == *str)
                return str - string;
        str++;
    }
    return str - string;
}


