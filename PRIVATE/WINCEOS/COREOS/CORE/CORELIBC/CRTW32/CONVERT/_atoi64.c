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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include <stdlib.h>

__int64 _atoi64(const char *nptr) {
    /* Performance note:  avoid char arithmetic on RISC machines. (4-byte regs) */
    int c;          /* current char */
    __int64 total;      /* current total */
    int sign;       /* if '-', then negative, otherwise positive */
    /* skip whitespace */
    while (isspace(*nptr))
        ++nptr;
    c = *nptr++;
    sign = c;       /* save sign indication */
    if (c == '-' || c == '+')
        c = *nptr++;    /* skip sign */
    total = 0;
    while (c >= '0' && c <= '9') {
        total = 10 * total + (c - '0');     /* accumulate digit */
        c = *nptr++;    /* get next char */
    }
    if (sign == '-')
        return -total;
    else
        return total;   /* return result, negated if necessary */
}

