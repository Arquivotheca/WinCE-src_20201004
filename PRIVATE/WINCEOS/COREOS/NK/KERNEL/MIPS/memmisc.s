//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include "mem_mips.h"



//------------------------------------------------------------------------------
//  void ZeroPage(void *vpPage)
//
//  Entry   (a0) = (vpPage) = ptr to address of page to zero
//  Return  none
//  Uses    a0, a1,t0
//------------------------------------------------------------------------------
LEAF_ENTRY(ZeroPage)
        .set noreorder
        
        lui     t0, 0xa000
        or      a0, t0                  // force into un-cached space
        li      a1, 1 << VA_PAGE        // (a1) = # of bytes to zero out
#if     _MIPS64 
10:	sd	zero, (a0)
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
10:	sw	zero, (a0)
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



//------------------------------------------------------------------------------
//  unsigned strlenW(LPWSTR *str)
//
//  Entry   (a0) = (ptr) = ptr to string
//  Return  (v0) = length of string
//  Uses    t0
//------------------------------------------------------------------------------
LEAF_ENTRY(strlenW)
        .set noreorder
        
        move    v0, a0                  // (v0) = ptr to start of string
10:     lh      t0, (v0)                
        bne     t0, zero, 10b           
        add     v0, 2                   // (v0) = ptr to next byte
        sub     v0, 2                   // (v0) = ptr to last byte of string
        sub     v0, a0                  // (v0) = length of the string
        j       ra
        srl     v0, 1
        
        .end strlenW

#if 0
//
// use the crt version of strlen, like all the other CPU
//

//------------------------------------------------------------------------------
//  unsigned strlen(void *dst, void *src, uint size)
//
//  Entry   (a0) = (ptr) = ptr to string
//  Return  (v0) = length of string
//  Uses    t0
//------------------------------------------------------------------------------
LEAF_ENTRY(strlen)
        .set noreorder
        
        move    v0, a0                  // (v0) = ptr to start of string
10:     lb      t0, (v0)
        bne     t0, zero, 10b
        add     v0, 1                   // (v0) = ptr to next byte
        sub     v0, 1                   // (v0) = ptr to last byte of string
        j       ra                      
        sub     v0, a0                  // (v0) = length of the string
        
        .end strlen

#endif

//------------------------------------------------------------------------------
//  unsigned strcmpW(LPWSTR *s1, LPWSTR *s2)
//
//  Entry   (a0) = ptr to s1
//      (a1) = ptr to s2
//  Uses    a0, a1, t0, t1
//------------------------------------------------------------------------------
LEAF_ENTRY(strcmpW)
        .set noreorder
        
        lh      t0, (a0)
10:             
        lh      t1, (a1)
        add     a0, 2                   // fill delay slot with pointer move
        sub     v0,t0,t1
        bne     v0, zero, 20f           // if not equal, return
        add     a1, 2                   // fill delay slot with ptr move
        bne     t1, zero, 10b           // if not zero, continue
        lh      t0, (a0)                // fill delay slot with load
20:             
        j       ra
        nop
        
        .end strcmpW


#if 0
//
// use the crt version of strcmp, like all the other CPU
//

//------------------------------------------------------------------------------
//  unsigned strcmp(void *s1, void *s2)
//
//  Entry   (a0) = ptr to s1
//      (a1) = ptr to s2
//  Uses    a0, a1, t0, t1
//------------------------------------------------------------------------------
LEAF_ENTRY(strcmp)
        .set noreorder
        
        lb      t0, (a0)
10:             
        lb      t1, (a1)
        add     a0, 1                   // fill delay slot with pointer move
        sub     v0,t0,t1
        bne     v0, zero, 20f           // if not equal, return
        add     a1, 1                   // fill delay slot with ptr move
        bne     t1, zero, 10b           // if not zero, continue
        lb      t0, (a0)                // fill delay slot with load
20:             
        j       ra
        nop
        
        .end strcmp

#endif