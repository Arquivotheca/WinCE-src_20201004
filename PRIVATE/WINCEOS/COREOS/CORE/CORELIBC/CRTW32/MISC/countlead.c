//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>
#include <limits.h>
#include <cmnintrin.h>

#pragma warning(disable:4163)
#pragma function (_CountLeadingZeros)

//
// _CountLeadingZeros
//
unsigned _CountLeadingZeros(long value)
{
    unsigned long x = value;
    unsigned long y;
    unsigned i;
    unsigned n = 0;

    if (value == 0)
        return sizeof(value) * CHAR_BIT;

    for (i = (sizeof(value) * CHAR_BIT)/2; i != 0; i >>= 1)
    {
        // check upper half of remaining bits
        y = x >> i;
        if (y == 0)
            // if zero then count them
            n += i;
        else
            // otherwise check shifted bits
            x = y;
    }

    return n;
}

#pragma function (_CountLeadingZeros64)

//
// _CountLeadingZeros64
//
unsigned _CountLeadingZeros64(__int64 value)
{
    unsigned __int64 x = value;
    unsigned __int64 y;
    unsigned i;
    unsigned n = 0;

    if (value == 0)
        return sizeof(value) * CHAR_BIT;

    for (i = (sizeof(value) * CHAR_BIT)/2; i != 0; i >>= 1)
    {
        // check upper half of remaining bits
        y = x >> i;
        if (y == 0)
            // if zero then count them
            n += i;
        else
            // otherwise check shifted bits
            x = y;
    }

    return n;
}

#pragma function (_CountLeadingOnes)

//
// _CountLeadingOnes
//
unsigned _CountLeadingOnes(long value)
{
    // return leading zeros of bit inverted value
    return _CountLeadingZeros( ~value );
}

#pragma function (_CountLeadingOnes64)

//
// _CountLeadingOnes64
//
unsigned _CountLeadingOnes64(__int64 value)
{
    // return leading zeros of bit inverted value
    return _CountLeadingZeros64( ~value );
}

#pragma function (_CountLeadingSigns)

//
// _CountLeadingSigns
//
unsigned _CountLeadingSigns(long value)
{
    long sign = value >> ((sizeof(value) * CHAR_BIT) - 1);

    // xor with replicated sign bit to set bits that match the sign bit
    //  to zero and bits that differ to one.
    // then count the leading zeros and subtract one to get leading signs
    return _CountLeadingZeros( value ^ sign ) - 1;
}

#pragma function (_CountLeadingSigns64)

//
// _CountLeadingSigns64
//
unsigned _CountLeadingSigns64(__int64 value)
{
    __int64 sign = value >> ((sizeof(value) * CHAR_BIT) - 1);

    // xor with replicated sign bit to set bits that match the sign bit
    //  to zero and bits that differ to one.
    // then count the leading zeros and subtract one to get leading signs
    return _CountLeadingZeros64( value ^ sign ) - 1;
}
