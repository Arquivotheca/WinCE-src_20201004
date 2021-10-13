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

#define __ascii_isalpha(c)     ( ('A' <= (c) && (c) <= 'Z') || ( 'a' <= (c) && (c) <= 'z'))
#define __ascii_isdigit(c)     ( '0' <= (c) && (c) <= '9')
#define __ascii_toupper(c)     ( (((c) >= 'a') && ((c) <= 'z')) ? ((c) - 'a' + 'A') : (c) )
#define __ascii_iswalpha(c)    __ascii_isalpha(c)
#define __ascii_iswdigit(c)    __ascii_isdigit(c)
#define __ascii_towupper(c)    __ascii_toupper(c)

static int __ascii_isspace(int c) {
    switch (c)
    {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
        case '\f':
        case '\v':
            return 1;
        default:
            return 0;
    }
}

