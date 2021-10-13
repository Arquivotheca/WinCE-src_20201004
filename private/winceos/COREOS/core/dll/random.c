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
// --------------------------------------------------------------------------
//
//
// Module:
//
//    random.c
//
// Purpose:
//
//    Implementation of Random.
//
// --------------------------------------------------------------------------

#include <windows.h>

DWORD sr;

int RandBit() {
    if (!sr)
        if (0 == (sr = GetTickCount()))
            sr = 1;
    sr = ((((sr>>7) ^ (sr>>5) ^ (sr>>2) ^ (sr>>1) ^ sr) & 1) << 31) |
    (sr >> 1);
    return (sr & 1);
}

DWORD WINAPI Random () {
    DWORD res = 0, loop;
    for (loop = 0; loop < 32; loop++)
        res = ((res<<1) | RandBit());
    return res;
}

