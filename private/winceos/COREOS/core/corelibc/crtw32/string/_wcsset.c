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

#include <string.h>

#pragma warning(disable:4163)
#pragma function(_wcsset)

/***
*wchar_t *_wcsset(string, val) - sets all of string to val (wide-characters)
*
*Purpose:
*   Sets all of wchar_t characters in string (except the terminating '/0'
*   character) equal to val (wide-characters).
*
*
*Entry:
*   wchar_t *string - string to modify
*   wchar_t val - value to fill string with
*
*Exit:
*   returns string -- now filled with val's
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

wchar_t * _wcsset (wchar_t * string, wchar_t val) {
    wchar_t *start = string;
    while (*string)
        *string++ = val;
    return(start);
}

