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
/*++


Module Name:

    kdpcpu.h

Abstract:

    Machine specific kernel debugger data types and constants

Revision History:

--*/

#ifndef _KDPCPU_
#define _KDPCPU_

#ifdef MIPS

#define KDP_BREAKPOINT_TYPE         ULONG
#define KDP_BREAKPOINT_ALIGN        3
#define KDP_BREAKPOINT_VALUE        0x0001000D  // break 1

#if defined(MIPSII)
#define KDP_BREAKPOINT_32BIT_TYPE   ULONG
#define KDP_BREAKPOINT_32BIT_VALUE  0x0001000D  // break 1
#define KDP_BREAKPOINT_16BIT_TYPE   USHORT
#define KDP_BREAKPOINT_16BIT_VALUE  0xE825      // break 1
#endif

#elif SHx

#define KDP_BREAKPOINT_TYPE         USHORT
#define KDP_BREAKPOINT_ALIGN        1
#define KDP_BREAKPOINT_VALUE        0xC301      // trapa 1

#elif x86

#define KDP_BREAKPOINT_TYPE         UCHAR
#define KDP_BREAKPOINT_ALIGN        0
#define KDP_BREAKPOINT_VALUE        0xCC        // int 3

#elif ARM

#define KDP_BREAKPOINT_TYPE         ULONG
#define KDP_BREAKPOINT_ALIGN        3
#define KDP_BREAKPOINT_VALUE        0xE6000010  // undefined instruction

#if defined(THUMBSUPPORT)
#define KDP_BREAKPOINT_32BIT_TYPE   ULONG
#define KDP_BREAKPOINT_32BIT_VALUE  0xE6000010  // undefined instruction
#define KDP_BREAKPOINT_16BIT_TYPE   USHORT
#define KDP_BREAKPOINT_16BIT_VALUE  0xDEFE      // undefined instruction
#endif

#else
#error unknown cpu type
#endif

#endif // _KDPCPU_
