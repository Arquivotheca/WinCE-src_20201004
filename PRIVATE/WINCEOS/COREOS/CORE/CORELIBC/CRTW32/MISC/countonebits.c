//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>
#include <cmnintrin.h>

#pragma warning(disable:4163)
#pragma function (_CountOneBits)

//
// _CountOneBits
//
// Return number of bits set to one
unsigned _CountOneBits(long value) {
    // In-place adder tree: perform 16 1-bit adds, 8 2-bit adds, 4 4-bit adds,
    // 2 8=bit adds, and 1 16-bit add.
    // Modified according to Phil Bagwell's suggestion
    // from letter in Dr. Dobb's Journal Nov 2000
    // to reduce the number of masking operations
    value -= ((value >> 1)&0x55555555);
    value = ((value >> 2)&0x33333333) + (value&0x33333333);
    value = ((value >> 4)&0x0F0F0F0F) + (value&0x0F0F0F0F);
    value += (value >> 8);
    value += (value >>16);
    value &= 0xFF;
    return (unsigned)value;
}
