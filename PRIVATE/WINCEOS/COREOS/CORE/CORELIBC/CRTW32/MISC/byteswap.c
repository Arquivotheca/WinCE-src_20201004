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
#pragma function (_byteswap_ushort)

//
// _byteswap_ushort
//
// swap upper and lower bytes
unsigned short _byteswap_ushort(unsigned short value)
{
    return (value >> CHAR_BIT) | (value << CHAR_BIT);
}

#pragma function (_byteswap_ulong)

//
// _byteswap_ulong
//
// reverse byte order
unsigned long _byteswap_ulong(unsigned long value)
{
    return _byteswap_ushort((unsigned short)value) << (2 * CHAR_BIT) |
           _byteswap_ushort((unsigned short)(value >> (2 * CHAR_BIT)));
}

#pragma function (_byteswap_uint64)

//
// _byteswap_uint64
//
// reverse byte order
unsigned __int64 _byteswap_uint64(unsigned __int64 value)
{
    return (unsigned __int64)(_byteswap_ulong((unsigned long)value)) << (4 * CHAR_BIT) |
           (unsigned __int64)(_byteswap_ulong((unsigned long)(value >> (4 * CHAR_BIT))));
}
