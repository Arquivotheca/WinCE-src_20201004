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
#include <corecrt.h>

double __cdecl atof(const char *nptr) {
    const char *EndPtr;
    _LDBL12 ld12;
    double x;
    while (isspace((int)(unsigned char)*nptr))
        nptr++;
    if (__strgtold12(&ld12, &EndPtr, nptr) & SLD_NODIGITS)
        return 0.0;
    _ld12tod(&ld12, &x);
    return x;
}

