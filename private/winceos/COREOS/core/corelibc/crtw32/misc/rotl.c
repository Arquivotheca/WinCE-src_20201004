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
#include <corecrt.h>
#include <cmnintrin.h>

#pragma warning(disable:4163)
#pragma function(_rotl, _lrotl, _rotl64)

unsigned _rotl (unsigned val, int shift) {
    register unsigned hibit;    /* non-zero means hi bit set */
    register unsigned num = val;    /* number to rotate */
    shift &= 0x1f;            /* modulo 32 -- this will also make negative shifts work */
    while (shift--) {
        hibit = num & 0x80000000;  /* get high bit */
        num <<= 1;        /* shift left one bit */
        if (hibit)
            num |= 1;    /* set lo bit if hi bit was set */
    }

    return num;
}

unsigned long _lrotl(unsigned long val,    int shift) {
    return( (unsigned long) _rotl((unsigned) val, shift) );
}

unsigned __int64 _rotl64 (unsigned __int64 val, int shift) {
    unsigned __int64 hibit;                  /* non-zero means hi bit set */
    unsigned __int64 num = val;              /* number to rotate */
    shift &= 0x3f;                      /* modulo 64 -- this will also make
                                               negative shifts work */
    while (shift--) {
        hibit = num & 0x8000000000000000;  /* get high bit */
        num <<= 1;                         /* shift left one bit */
        if (hibit)
            num |= 0x1;                        /* set lo bit if hi bit was set */
    }

    return num;
}
