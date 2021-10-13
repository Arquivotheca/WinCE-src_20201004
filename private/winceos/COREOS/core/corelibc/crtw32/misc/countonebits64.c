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
#pragma function (_CountOneBits64)

//
// _CountOneBits64
//
// Return number of bits set to one
unsigned _CountOneBits64(__int64 u) {
    // In-place adder tree: perform 16 1-bit adds, 8 2-bit adds, 4 4-bit adds,
    // 2 8=bit adds, and 1 16-bit add.
    // Modified according to Phil Bagwell's suggestion
    // from letter in Dr. Dobb's Journal Nov 2000
    // to reduce the number of masking operations
    unsigned hi = (unsigned)(u >> 32);
    unsigned lo = (unsigned)(u);
    hi -= ((hi >> 1)&0x55555555);
    lo -= ((lo >> 1)&0x55555555);
    hi = ((hi >> 2)&0x33333333) + (hi&0x33333333);
    lo = ((lo >> 2)&0x33333333) + (lo&0x33333333);
    hi = ((hi >> 4)&0x0F0F0F0F) + (hi&0x0F0F0F0F);
    lo = ((lo >> 4)&0x0F0F0F0F) + (lo&0x0F0F0F0F);
    lo += hi;
    lo += (lo >> 8);
    lo += (lo >>16);
    lo &= 0xFF;
    return lo;
}
