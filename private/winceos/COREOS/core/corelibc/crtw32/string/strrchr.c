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

#if !defined(_M_IX86)

char * strrchr(const char * string, int ch) {
    char *start = (char *)string;

    /* find end of string */
    while (*string++);

    /* search towards front */
    while (--string != start && *string != (char) ch);

    if (*string == (char) ch)       /* char found ? */
        return( (char *)string );
    return(NULL);
}

#endif

