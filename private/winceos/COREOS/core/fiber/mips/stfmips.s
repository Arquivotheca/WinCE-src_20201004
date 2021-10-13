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
// Module Name:
//
//    stfmips.s
//
// Abstract:
//
//    This module implements the code to switch to another fiber
//
// Environment:
//
//
#include "ksmips.h"

#ifdef  _MIPS64
#define L_REG   ld
#define L_FREG  ldc1
#define S_REG   sd
#define S_FREG  sdc1

#define REG_SIZE        8   // register size is 8 bytes

#else   //  _MIPS64
#define L_REG   lw
#define L_FREG  lwc1
#define S_REG   sw
#define S_FREG  swc1

#define REG_SIZE        4   // register size is 4 bytes

#endif  //  _MIPS64

#define FX_CtxRa        0x0*REG_SIZE         // RA: must be first
#define FX_CtxA0        0x1*REG_SIZE         // arg1: must be second
#define FX_CtxA1        0x2*REG_SIZE         // arg2: must be third
#define FX_CtxS0        0x3*REG_SIZE
#define FX_CtxS1        0x4*REG_SIZE
#define FX_CtxS2        0x5*REG_SIZE
#define FX_CtxS3        0x6*REG_SIZE
#define FX_CtxS4        0x7*REG_SIZE
#define FX_CtxS5        0x8*REG_SIZE
#define FX_CtxS6        0x9*REG_SIZE
#define FX_CtxS7        0xa*REG_SIZE
#define FX_CtxS8        0xb*REG_SIZE
#define FX_CtxGp        0xc*REG_SIZE
#define FX_FPU_USED     0xd*REG_SIZE

#ifdef MIPS_HAS_FPU

#define FX_CtxF20       0xe*REG_SIZE
#define FX_CtxF22       0xf*REG_SIZE
#define FX_CtxF24       0x10*REG_SIZE
#define FX_CtxF26       0x11*REG_SIZE
#define FX_CtxF28       0x12*REG_SIZE
#define FX_CtxF30       0x13*REG_SIZE

// total number of registers
#define NUM_REGS        0x14

#else

// total number of registers
#define NUM_REGS        0xe

#endif

// totol size of the context
#define SIZE_FIBERCTX   (NUM_REGS * REG_SIZE) 

// where to find stack base/bound and cur-fiber from TLS pointer
//  (must match the PRETLS definitions in pkfuncs.h)
#define PRETLS_BASE     -4
#define PRETLS_BOUND    -8
#define PRETLS_CURFIBER -12
#define PRETLS_SIZE     -16

// the fiber fields (offset in FIBERSTRUCT)
#define FBS_THRD        0x0
#define FBS_BASE        0x4
#define FBS_BOUND       0x8
#define FBS_SP          0xc
#define FBS_SIZE        0x10

    .data

dwSizeCtx:  .word   SIZE_FIBERCTX

    .globl dwSizeCtx

    .text
    
        SBTTL("Switch To Fiber")
//------------------------------------------------------------------------------
//
// void
// MDFiberSwitch (
//    IN OUT PFIBERSTRUCT pFiberTo,
//    IN DWORD dwThrdId,
//    IN OUT LPDWORD pTlsPtr,
//    IN BOOL fFpuUsed
//    )
//
// Routine Description:
//
//    This routine is called to switch from a fiber to another fiber
//
// Arguments:
//
//    (a0) - pointer to the fiber to switch to
//    (a1) - current thread id
//    (a2) - TLS pointer
//    (a3) - if FPU is used
//
// Return Value:
//
//    None
//
//------------------------------------------------------------------------------
LEAF_ENTRY(MDFiberSwitch)
        .set noreorder
        subu    sp, SIZE_FIBERCTX       // (sp) = ptr to FIBERCONTEXT buffer

        // save the permanant registers and return address
        S_REG   s0, FX_CtxS0(sp)
        S_REG   s1, FX_CtxS1(sp)
        S_REG   s2, FX_CtxS2(sp)
        S_REG   s3, FX_CtxS3(sp)
        S_REG   s4, FX_CtxS4(sp)
        S_REG   s5, FX_CtxS5(sp)
        S_REG   s6, FX_CtxS6(sp)
        S_REG   s7, FX_CtxS7(sp)
        S_REG   s8, FX_CtxS8(sp)
        S_REG   gp, FX_CtxGp(sp)
        S_REG   ra, FX_CtxRa(sp)
#ifdef MIPS_HAS_FPU
        S_REG   a3, FX_FPU_USED(sp)
        beqz    a3, nosavefp
        nop
        // save permanant floating point registers
        S_FREG  f20, FX_CtxF20(sp)
        S_FREG  f22, FX_CtxF22(sp)
        S_FREG  f24, FX_CtxF24(sp)
        S_FREG  f26, FX_CtxF26(sp)
        S_FREG  f28, FX_CtxF28(sp)
        S_FREG  f30, FX_CtxF30(sp)
nosavefp:
#endif
        // update the TLS and fiber informations
        lw      t2, PRETLS_CURFIBER(a2) // (t2) = current fiber

        // save the stack pointer in the current fiber structure
        sw      sp, FBS_SP(t2)
        // save stackbound in the current fiber structure
        lw      t1, PRETLS_BOUND(a2)    // (t1) = current stack bound
        sw      t1, FBS_BOUND(t2)       // save the current stack bound to fiber structure
                                        // (stack base should remain unchnaged)
        // update TLS using the new fiber
        lw      t1, FBS_BASE(a0)        // (t1) = stack base of new fiber
        srl     t1, 1                   // mask off the LSB (main fiber indicator)
        sll     t1, 1
        lw      t3, FBS_BOUND(a0)       // (t3) = stack bound of new fiber
        lw      t4, FBS_SIZE(a0)        // (t4) = stack size of new fiber
        sw      a0, PRETLS_CURFIBER(a2) // update current fiber
        sw      t1, PRETLS_BASE(a2)     // save stack base in TLS
        sw      t3, PRETLS_BOUND(a2)    // save stack bound in TLS
        sw      t4, PRETLS_SIZE(a2)     // save stack size in TLS

        // restore the sp of the fiber to switch to
        lw      sp, FBS_SP(a0)

        // update fiber thread id
        sw      zero, FBS_THRD(t2)      // clean the hThrd field so other fibers can switch to it
        sw      a1, FBS_THRD(a0)        // save thread id to the new fiber

        // restore the permanant registers and return address
        L_REG   s0, FX_CtxS0(sp)
        L_REG   s1, FX_CtxS1(sp)
        L_REG   s2, FX_CtxS2(sp)
        L_REG   s3, FX_CtxS3(sp)
        L_REG   s4, FX_CtxS4(sp)
        L_REG   s5, FX_CtxS5(sp)
        L_REG   s6, FX_CtxS6(sp)
        L_REG   s7, FX_CtxS7(sp)
        L_REG   s8, FX_CtxS8(sp)
        L_REG   gp, FX_CtxGp(sp)
        L_REG   ra, FX_CtxRa(sp)

#ifdef MIPS_HAS_FPU
        L_REG   t0, FX_FPU_USED(sp)
        beqz    t0, norestorefp
        nop
        // restore permanant floating point registers
        L_FREG  f20, FX_CtxF20(sp)
        L_FREG  f22, FX_CtxF22(sp)
        L_FREG  f24, FX_CtxF24(sp)
        L_FREG  f26, FX_CtxF26(sp)
        L_FREG  f28, FX_CtxF28(sp)
        L_FREG  f30, FX_CtxF30(sp)
norestorefp:
#endif

        // restore a0 and a1 (FiberBaseProc's argument)
        // NOTE: we don't really have to save the arguments, but two instructions is
        //      a small price to pay comparing to have to implement FiberBaseFunc in
        //      assembly
        L_REG   a0, FX_CtxA0(sp)
        L_REG   a1, FX_CtxA1(sp)
        
        // return to the new fiber and restore the original sp
        jr      ra
        addu    sp, SIZE_FIBERCTX

        .end    MDFiberSwitch


