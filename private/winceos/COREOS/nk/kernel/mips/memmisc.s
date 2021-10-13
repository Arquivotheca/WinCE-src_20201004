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
//------------------------------------------------------------------------------
//      TITLE("Memory manipulation routines")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    memmisc.s
//
// Abstract:
//
//    This module implements various functions for initializing and
// copying memory.
//
// Environment:
//
//    Kernel mode or user mode.
//
//------------------------------------------------------------------------------
#include "ksmips.h"
#define ASM_ONLY
#include "vmlayout.h"



//------------------------------------------------------------------------------
//  void ZeroPage(void *vpPage)
//
//  Entry   (a0) = (vpPage) = ptr to address of page to zero
//  Return  none
//  Uses    a0, a1,t0
//------------------------------------------------------------------------------
LEAF_ENTRY(ZeroPage)
        .set noreorder

        li      a1, VM_PAGE_SIZE        // (a1) = # of bytes to zero out
#if     _MIPS64 
10:	    sd	zero, (a0)
    	sd	zero, 8(a0)
    	sd	zero, 16(a0)
    	sd	zero, 24(a0)
    	sd	zero, 32(a0)
    	sd	zero, 40(a0)
    	sd	zero, 48(a0)
    	sd	zero, 56(a0)
    	sub	a1, 64			// (a1) = # of bytes left to zero
    	bne	zero, a1, 10b
    	add	a0, 64			// (a0) = next 32 byte chunk to zero
#else   //  _MIPS64
10:	    sw	zero, (a0)
    	sw	zero, 4(a0)
    	sw	zero, 8(a0)
    	sw	zero, 12(a0)
    	sw	zero, 16(a0)
    	sw	zero, 20(a0)
    	sw	zero, 24(a0)
    	sw	zero, 28(a0)
    	sub	a1, 32		// (a1) = # of bytes left to zero
    	bne	zero, a1, 10b
    	add	a0, 32		// (a0) = next 32 byte chunk to zero
#endif  //  _MIPS64
        j       ra
        nop
        .end ZeroPage
