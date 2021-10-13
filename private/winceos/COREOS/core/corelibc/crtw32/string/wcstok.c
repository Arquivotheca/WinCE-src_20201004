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
#include <cruntime.h>

/***
*wchar_t *wcstok(string, control) - tokenize string with delimiter in control
*   (wide-characters)
*
*Purpose:
*   wcstok considers the string to consist of a sequence of zero or more
*   text tokens separated by spans of one or more control chars. the first
*   call, with string specified, returns a pointer to the first wchar_t of
*   the first token, and will write a null wchar_t into string immediately
*   following the returned token. subsequent calls with zero for the first
*   argument (string) will work thru the string until no tokens remain. the
*   control string may be different from call to call. when no tokens remain
*   in string a NULL pointer is returned. remember the control chars with a
*   bit map, one bit per wchar_t. the null wchar_t is always a control char
*   (wide-characters).
*
*Entry:
*   wchar_t *string - wchar_t string to tokenize, or NULL to get next token
*   wchar_t *control - wchar_t string of characters to use as delimiters
*
*Exit:
*   returns pointer to first token in string, or if string
*   was NULL, to next token
*   returns NULL when no more tokens remain.
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

wchar_t* __cdecl wcstok (
    wchar_t * string,
    const wchar_t * control
    )
{
    wchar_t *token;
    const wchar_t *ctl;
    wchar_t** ppnexttoken = __crt_get_storage_wcstok();
    if (ppnexttoken == NULL)
        return NULL;

    /* If string==NULL, continue with previous string */
    if (!string)
        string = *ppnexttoken;

    /* Find beginning of token (skip over leading delimiters). Note that
     * there is no token iff this loop sets string to point to the terminal
     * null (*string == '\0') */

    while (*string) {
        for (ctl=control; *ctl && *ctl != *string; ctl++)
            ;
        if (!*ctl) break;
        string++;
    }

    token = string;

    /* Find the end of the token. If it is not the end of the string,
     * put a null there. */
    for ( ; *string ; string++ ) {
        for (ctl=control; *ctl && *ctl != *string; ctl++)
            ;
        if (*ctl) {
            *string++ = '\0';
            break;
        }
    }

    /* Update nextoken */
    *ppnexttoken = string;

    /* Determine if a token has been found. */
    if (token == string)
        return NULL;
    else
        return token;
}

