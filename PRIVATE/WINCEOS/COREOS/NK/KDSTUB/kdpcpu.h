//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
