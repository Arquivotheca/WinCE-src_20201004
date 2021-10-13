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
#pragma function (_MulUnsignedHigh)

//
// _MulUnsignedHigh
//
unsigned long _MulUnsignedHigh ( unsigned long x, unsigned long y)
{
	unsigned __int64 x64 = x;
    unsigned __int64 y64 = y;

    return (unsigned long)((x64 * y64) >> (sizeof(unsigned long) * CHAR_BIT));
}
