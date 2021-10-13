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
//==========================================================================;
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//
//--------------------------------------------------------------------------;
//
//  muldiv32.h
//
//  Description:
//      math routines for 32 bit signed and unsiged numbers.
//
//      MulDiv32(a,b,c) = (a * b) / c         (round down, signed)
//
//      MulDivRD(a,b,c) = (a * b) / c         (round down, unsigned)
//      MulDivRN(a,b,c) = (a * b + c/2) / c   (round nearest, unsigned)
//      MulDivRU(a,b,c) = (a * b + c-1) / c   (round up, unsigned)
//
//==========================================================================;

#ifndef _INC_MULDIV32
#define _INC_MULDIV32


#ifndef INLINE
#define INLINE __inline
#endif


//
//  Use C9 __int64 support for Daytona RISC platforms.
//

INLINE LONG MulDiv32( LONG a, LONG b, LONG c )
{
    return  (LONG)( (a * b) / c );
}


INLINE DWORD MulDivRD( DWORD a, DWORD b, DWORD c )
{
    return (DWORD)( (a * b) / c );
}


INLINE DWORD MulDivRN( DWORD a, DWORD b, DWORD c )
{
    return (DWORD)( ((a * b) + c/2) / c );
}


INLINE DWORD MulDivRU( DWORD a, DWORD b, DWORD c )
{
    return (DWORD)( ((a * b) + c-1) / c );
}


//
//  some code references these by other names.
//
#define muldiv32    MulDivRN
#define muldivrd32  MulDivRD
#define muldivru32  MulDivRU

#endif  // _INC_MULDIV32
