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
#include <cruntime.h>

char * __cdecl strtok (
    char * string,
    const char * control
    )
{
    unsigned char *str;
    const unsigned char *ctrl = (const unsigned char *)control;
    unsigned char map[32];
    int count;

    char** ppnexttoken = __crt_get_storage_strtok();
    if (ppnexttoken == NULL)
        return NULL;

    /* Clear control map */
    for (count = 0; count < 32; count++)
        map[count] = 0;

    /* Set bits in delimiter table */
    do {
        map[*ctrl >> 3] |= (1 << (*ctrl & 7));
    } while (*ctrl++);

    /* Initialize str */

    /* If string is NULL, set str to the saved
     * pointer (i.e., continue breaking tokens out of the string
     * from the last strtok call) */
    if (string)
        str = (unsigned char *)string;
    else
        str = (unsigned char *)*ppnexttoken;

    /* Find beginning of token (skip over leading delimiters). Note that
     * there is no token iff this loop sets str to point to the terminal
     * null (*str == '\0') */
    while ((map[*str >> 3] & (1 << (*str & 7))) && *str)
        str++;

    string = (char *)str;

    /* Find the end of the token. If it is not the end of the string,
     * put a null there. */
    for (; *str ; str++)
        if (map[*str >> 3] & (1 << (*str & 7))) {
            *str++ = '\0';
            break;
        }

    /* Update nextoken */
    *ppnexttoken = (char *)str;

    /* Determine if a token has been found. */
    if (string == (char *)str)
        return NULL;
    else
        return string;
}

