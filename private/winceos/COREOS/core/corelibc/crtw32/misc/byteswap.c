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
